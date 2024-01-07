#include "Serialization.h"
#include "System.h"

void Serialization::AddTracker(RE::TESGlobal* a_global, RE::BGSLocation* a_region)
{
    logs::info("Serialization::AddTracker :: Parsing tracker: '{}' with global variable: '{:x}'", a_region->GetName(), a_global->GetFormID());

    Tracker instance{a_global, a_region};
    trackers.push_back(std::make_shared<Tracker>(instance));
    logs::info("Serialization::AddTracker :: Current number of trackers: '{}'", trackers.size());
}

void Serialization::ClearTracker(RE::BGSLocation* a_region)
{
    logs::info("Serialization::ClearTracker :: Searching for tracker: '{}'", a_region->GetName());
    for (auto& tracker : trackers) {
        logs::info("Serialization::ClearTracker :: Parsing tracker: '{}'", tracker->region->GetName());
        if (tracker->region == a_region) {
            logs::info("Serialization::ClearTracker :: Tracker found!");
            tracker->global->value = 0U;
            logs::info("Serialization::ClearTracker :: Tried to clear global variable: '{:x}'. Result value: '{}'", tracker->global->GetFormID(), tracker->global->value);
            tracker->reward.clear();
            logs::info("Serialization::ClearTracker :: Tried to clear reward counter. Result value: '{}'", tracker->reward.size());
            break;
        }
    }
}

void Serialization::DeserializeObjectivesText(RE::TESQuest* a_quest, RE::BGSLocation* a_location, std::uint16_t a_index, const std::string a_text)
{
    auto quests = System::GetSingleton()->GetQuests();

    for (auto& quest : quests) {
        if (quest->quest == a_quest) {
            if (quest->location == a_location) {
                quest->objectiveIndex = a_index;
            }

            for (auto& objective : quest->quest->objectives) {
                if (objective->index == a_index) {
                    objective->displayText = a_text;
                }
            }

            if (!IsObjectiveSerialized(a_location)) {
                objectives.push_back(std::make_unique<Objective>(a_quest, a_location, a_index, a_text));
            }
        }
    }
}

auto Serialization::GetTrackers() const -> const std::vector<std::shared_ptr<Tracker>>&
{
    return trackers;
}

bool Serialization::IsLocationReserved(RE::BGSLocation* a_location) const
{
    return std::find_if(reservedLocations.begin(), reservedLocations.end(), [a_location](RE::BGSLocation* location) { return location == a_location; }) != reservedLocations.end();
}

bool Serialization::IsObjectiveSerialized(RE::BGSLocation* a_location) const
{
    return std::find_if(objectives.begin(), objectives.end(), [a_location](std::shared_ptr<Serialization::Objective> objective) { return objective->location == a_location; }) != objectives.end();
}

bool Serialization::IsTrackerSerialized(RE::TESGlobal* a_global) const
{
    return std::find_if(trackers.begin(), trackers.end(), [a_global](std::shared_ptr<Serialization::Tracker> tracker) { return tracker->global == a_global; }) != trackers.end();
}

// Credits to Papyrus Extender by powerofthree for the string helper functions.
bool Serialization::ReadString(SKSE::SerializationInterface* a_interface, std::string& a_string) const
{
    std::size_t size = 0;

    if (!a_interface->ReadRecordData(size)) {
        return false;
    }

    a_string.reserve(size);

    if (!a_interface->ReadRecordData(a_string.data(), static_cast<std::uint32_t>(size))) {
        return false;
    }
    return true;    
}

void Serialization::ReserveLocation(RE::BGSLocation* a_location, bool a_reserve)
{
    if (a_location) {
        if (a_reserve) {
            logs::info("Serialization::ReserveLocation :: Reserving Location: '{}'", a_location->GetName());
            reservedLocations.push_back(a_location);
        } else {
            logs::info("Serialization::ReserveLocation :: Releasing Location: '{}'", a_location->GetName());
            reservedLocations.erase(std::remove(reservedLocations.begin(), reservedLocations.end(), a_location), reservedLocations.end());
        }
    }
}

void Serialization::SerializeObjectivesText(RE::TESQuest* a_quest, RE::BGSLocation* a_location, std::uint16_t a_index, std::string a_text)
{
    objectives.push_back(std::make_shared<Objective>(a_quest, a_location, a_index, a_text));
}

void Serialization::SetTracker(RE::BGSLocation* a_region, Util::DIFFICULTY a_difficulty, std::uint32_t a_amount)
{
    logs::info("Serialization::SetTracker :: Searching for tracker: '{}'", a_region->GetName());
    for (auto& tracker : trackers) {
        logs::info("Serialization::SetTracker :: Parsing tracker: '{}'", tracker->region->GetName());
        if (tracker->region == a_region) {
            logs::info("Serialization::SetTracker :: Tracker found!");
            tracker->global->value = 1U;
            logs::info("Serialization::SetTracker :: Tried to set global variable: '{:x}'. Result value: '{}'", tracker->global->GetFormID(), tracker->global->value);
            tracker->reward[a_difficulty] += a_amount;
            logs::info("Serialization::SetTracker :: Tried to set reward counter for difficulty: '{}' with an increase of: '{}'. Result value: '{}'", static_cast<std::uint32_t>(a_difficulty), a_amount, tracker->reward[a_difficulty]);
            break;
        }
    }
}

bool Serialization::UpdateTracker(RE::TESGlobal* a_global, RE::BGSLocation* a_region, std::unordered_map<Util::DIFFICULTY, std::uint32_t>& a_reward)
{
    if (a_global && a_region) {
        for (auto& tracker : trackers) {
            if (tracker->global == a_global && tracker->region == a_region) {
                tracker->reward = a_reward;
                logs::info("Serialization::UpdateTracker :: Updated tracker '{:x}' for region '{}'", a_global->GetFormID(), a_region->GetName());
                return true;
            }
        }
    }
    logs::error("Serialization::UpdateTracker :: Failed to find tracker '{:x}' for region '{}'", a_global->GetFormID(), a_region->GetName());
    return false;
}

bool Serialization::WriteString(SKSE::SerializationInterface* a_interface, const std::string& a_string) const
{
    std::size_t size = a_string.length() + 1;
    return a_interface->WriteRecordData(size) && a_interface->WriteRecordData(a_string.data(), static_cast<std::uint32_t>(size));
}

bool Serialization::SaveObjectives(SKSE::SerializationInterface* a_interface)
{
    if (!a_interface->OpenRecord(kObjectives, kVersion)) {
        logs::error("Serialization::SaveObjectives :: Failed to read record data.");
        return false;
    }

    std::unique_lock lock(GetSingleton()->lock);

    auto& objectivesList = GetSingleton()->objectives;
    const auto count = objectivesList.size();

    if (!a_interface->WriteRecordData(&count, sizeof(count))) {
        logs::error("Serialization::SaveObjectives :: Failed to write record data with the number of objectives! '{}'", count);
        return false;
    }

    for (auto& objective : objectivesList) {
        const auto quest = objective->quest->GetFormID();
        const auto location = objective->location->GetFormID();
        const auto index = objective->index;
        const auto text = objective->text.data();

        if (!a_interface->WriteRecordData(&quest, sizeof(quest))) {
            logs::error("Serialization::SaveObjectives :: Failed to write record data with the quest formID! '{:x}'", quest);
            return false;
        }

        if (!a_interface->WriteRecordData(&location, sizeof(location))) {
            logs::error("Serialization::SaveObjectives :: Failed to write record data with the location formID! '{:x}'", location);
            return false;
        }

        if (!a_interface->WriteRecordData(&index, sizeof(index))) {
            logs::error("Serialization::SaveObjectives :: Failed to write record data with the index! '{}'", index);
            return false;
        }

        if (!GetSingleton()->WriteString(a_interface, text)) {
            logs::error("Serialization::SaveObjectives :: Failed to write record data with with the string! '{}'", text);
            return false;
        }
    }

    return true;
}

bool Serialization::SaveLocations(SKSE::SerializationInterface* a_interface)
{
    if (!a_interface->OpenRecord(kReservedLocations, kVersion)) {
        logs::error("Serialization::SaveLocations :: Failed to read record data.");
        return false;
    }

    std::unique_lock lock(GetSingleton()->lock);

    auto& locations = GetSingleton()->reservedLocations;
    const auto count = locations.size();

    if (!a_interface->WriteRecordData(&count, sizeof(count))) {
        logs::error("Serialization::SaveLocations :: Failed to write record data with the number of reserved locations! '{}'", count);
        return false;
    }

    for (auto& location : locations) {
        const auto formID = location->GetFormID();
        if (!a_interface->WriteRecordData(&formID, sizeof(formID))) {
            logs::error("Serialization::SaveLocations :: Failed to write record data with the reserved location formID! '{:x}'", formID);
            return false;
        }
    }

    return true;
}

bool Serialization::SaveTrackers(SKSE::SerializationInterface* a_interface)
{
    if (!a_interface->OpenRecord(kTrackers, kVersion)) {
        logs::error("Serialization::SaveTrackers :: Failed to read record data.");
        return false;
    }

    std::unique_lock lock(GetSingleton()->lock);

    auto& trackersList = GetSingleton()->GetTrackers();
    const auto count = trackersList.size();

    if (!a_interface->WriteRecordData(&count, sizeof(count))) {
        logs::error("Serialization::SaveTrackers :: Failed to write record data with the number of trackers! '{}'", count);
        return false;
    }

    for (auto& tracker : trackersList) {
        const auto global = tracker->global->GetFormID();
        const auto region = tracker->region->GetFormID();
        
        if (!a_interface->WriteRecordData(&global, sizeof(global))) {
            logs::error("Serialization::SaveTrackers :: Failed to write record data with the global variable formID! '{:x}'", global);
            return false;
        }

        if (!a_interface->WriteRecordData(&region, sizeof(region))) {
            logs::error("Serialization::SaveTrackers :: Failed to write record data with the region formID! '{:x}'", region);
            return false;
        }

        auto& rewards = tracker->reward;
        const auto amount = rewards.size();

        if (!a_interface->WriteRecordData(&amount, sizeof(amount))) {
            logs::error("Serialization::SaveTrackers :: Failed to write record data with the number of rewards! '{}'", amount);
            return false;
        }

        for (auto& reward : rewards) {
            const auto difficulty = reward.first;
            const auto quantity = reward.second;

            if (!a_interface->WriteRecordData(&difficulty, sizeof(difficulty))) {
                logs::error("Serialization::SaveTrackers :: Failed to write record data with the reward difficulty value! '{}'", static_cast<std::uint32_t>(difficulty));
                return false;
            }

            if (!a_interface->WriteRecordData(&quantity, sizeof(quantity))) {
                logs::error("Serialization::SaveTrackers :: Failed to write record data with the reward quantity value! '{}'", quantity);
                return false;
            }
        }
    }

    return true;
}

bool Serialization::LoadObjectives(SKSE::SerializationInterface* a_interface)
{
    std::size_t count;
    a_interface->ReadRecordData(&count, sizeof(count));
    logs::info("Serialization::LoadObjectives :: Loading '{}' objectives.", count);

    for (std::size_t i = 0; i < count; i++) {
        RE::FormID oldQuest;
        RE::FormID oldLocation;
        std::uint16_t index;
        std::string text;

        a_interface->ReadRecordData(&oldQuest, sizeof(oldQuest));
        a_interface->ReadRecordData(&oldLocation, sizeof(oldLocation));
        a_interface->ReadRecordData(&index, sizeof(index));
        GetSingleton()->ReadString(a_interface, text);

        RE::FormID newQuest;
        RE::FormID newLocation;

        if (!a_interface->ResolveFormID(oldQuest, newQuest)) {
            logs::error("Serialization::LoadObjectives :: Failed to resolve the quest formID! '{:x}' -> '{:x}'", oldQuest, newQuest);
            continue;
        }

        if (!a_interface->ResolveFormID(oldLocation, newLocation)) {
            logs::error("Serialization::LoadObjectives :: Failed to resolve the location formID! '{:x}' -> '{:x}'", oldLocation, newLocation);
            continue;
        }

        const auto quest = RE::TESForm::LookupByID<RE::TESQuest>(newQuest);
        const auto location = RE::TESForm::LookupByID<RE::BGSLocation>(newLocation);

        if (quest && location) {
            GetSingleton()->DeserializeObjectivesText(quest, location, index, text.data());
        } else {
            logs::error("Serialization::LoadObjectives");
            continue;
        }
    }

    return true;
}

bool Serialization::LoadLocations(SKSE::SerializationInterface* a_interface)
{
    std::size_t count;
    a_interface->ReadRecordData(&count, sizeof(count));
    logs::info("Serialization::LoadLocations :: Loading '{}' locations.", count);

    for (std::size_t i = 0; i < count; i++) {
        RE::FormID oldLocation;
        a_interface->ReadRecordData(&oldLocation, sizeof(oldLocation));
        RE::FormID newLocation;

        if (!a_interface->ResolveFormID(oldLocation, newLocation)) {
            logs::error("Serialization::LoadLocations :: Failed to resolve the location formID! '{:x}' -> '{:x}'", oldLocation, newLocation);
            continue;
        }

        const auto location = RE::TESForm::LookupByID<RE::BGSLocation>(newLocation);

        if (location) {
            GetSingleton()->ReserveLocation(location, true);
        } else {
            logs::error("Serialization::LoadLocations");
            continue;
        }
    }

    return true;
}

bool Serialization::LoadTrackers(SKSE::SerializationInterface* a_interface)
{
    std::size_t count;
    a_interface->ReadRecordData(&count, sizeof(count));
    logs::info("Serialization::LoadTrackers :: Loading '{}' trackers.", count);

    for (std::size_t i = 0; i < count; i++) {
        RE::FormID oldGlobal;
        RE::FormID oldRegion;
        std::size_t amount;
        std::unordered_map<Util::DIFFICULTY, std::uint32_t> temporary;

        a_interface->ReadRecordData(&oldGlobal, sizeof(oldGlobal));
        a_interface->ReadRecordData(&oldRegion, sizeof(oldRegion));
        a_interface->ReadRecordData(&amount, sizeof(amount));

        for (std::size_t j = 0; j < amount; j++) {
            Util::DIFFICULTY difficulty;
            std::uint32_t quantity;

            a_interface->ReadRecordData(&difficulty, sizeof(difficulty));
            a_interface->ReadRecordData(&quantity, sizeof(quantity));

            temporary.try_emplace(difficulty, quantity);
        }

        RE::FormID newGlobal;
        RE::FormID newRegion;

        if (!a_interface->ResolveFormID(oldGlobal, newGlobal)) {
            logs::error("Serialization::LoadTrackers :: Failed to resolve the global variable formID! '{:x}' -> '{:x}'", oldGlobal, newGlobal);
            continue;
        }

        if (!a_interface->ResolveFormID(oldRegion, newRegion)) {
            logs::error("Serialization::LoadTrackers :: Failed to resolve the region formID! '{:x}' -> '{:x}'", oldRegion, newRegion);
            continue;
        }

        const auto global = RE::TESForm::LookupByID<RE::TESGlobal>(newGlobal);
        const auto region = RE::TESForm::LookupByID<RE::BGSLocation>(newRegion);

        if (global && region) {
            if (!GetSingleton()->UpdateTracker(global, region, temporary)) {
                continue;
            }
        } else {
            logs::error("Serialization::LoadTrackers");
            continue;
        }
    }

    return true;
}

void Serialization::OnGameSaved(SKSE::SerializationInterface* a_interface)
{
    GetSingleton()->SaveObjectives(a_interface);
    GetSingleton()->SaveLocations(a_interface);
    GetSingleton()->SaveTrackers(a_interface);
}

void Serialization::OnGameLoaded(SKSE::SerializationInterface* a_interface)
{
    std::uint32_t type;
    std::uint32_t version;
    std::uint32_t length;

    while (a_interface->GetNextRecordInfo(type, version, length)) {
        if (version != kVersion) {
            logs::critical("Serialization::OnGameLoaded :: Record data version mismatch! '{}' -> '{}'", version, static_cast<std::uint32_t>(kVersion));
            continue;
        }

        switch (type) {
        case kReservedLocations:
            logs::info("Serialization::OnGameLoaded :: kReservedLocations");
            GetSingleton()->LoadLocations(a_interface);
            break;
        case kObjectives:
            logs::info("Serialization::OnGameLoaded :: kObjectives");
            GetSingleton()->LoadObjectives(a_interface);
            break;
        case kTrackers:
            logs::info("Serialization::OnGameLoaded :: kTrackers");
            GetSingleton()->LoadTrackers(a_interface);
            break;
        }
    }
}

void Serialization::OnRevert(SKSE::SerializationInterface*)
{
    logs::info("Serialization::OnRevert :: Reverting data.");
    std::unique_lock lock(GetSingleton()->lock);
    GetSingleton()->reservedLocations.clear();
    GetSingleton()->objectives.clear();

    for (auto& tracker : GetSingleton()->trackers) {
        tracker->reward.clear();
    }
}
