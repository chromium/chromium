// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/manual_testing_import.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

// Util struct for storing the list of profiles, credit cards and entities to be
// imported. If any of `profiles`, `credit_cards` or `entities` are
// std::nullopt, then the data used for import is malformed, and this will cause
// an error to be logged. When any of `profiles`, `credit_cards` or `entities`
// is empty, it means that the JSON file used for import did not include the
// corresponding key. It'll be treated as valid but won't be imported so that
// existing data in the data managers isn't cleared without replacement.
struct AutofillImportData {
  std::optional<std::vector<AutofillProfile>> profiles;
  std::optional<std::vector<CreditCard>> credit_cards;
  std::optional<std::vector<EntityInstance>> entities;
};

constexpr std::string_view kKeyProfiles = "profiles";
constexpr std::string_view kKeyCreditCards = "credit-cards";
constexpr std::string_view kKeyEntities = "entities";
constexpr std::string_view kKeyRecordType = "record_type";
constexpr std::string_view kKeyNickname = "nickname";
constexpr std::string_view kKeyEntityType = "entity_type";
constexpr std::string_view kKeyAttributes = "attributes";
constexpr auto kRecordTypeMapping =
    base::MakeFixedFlatMap<std::string_view, AutofillProfile::RecordType>(
        {{"account", AutofillProfile::RecordType::kAccount},
         {"accountHome", AutofillProfile::RecordType::kAccountHome},
         {"accountWork", AutofillProfile::RecordType::kAccountWork},
         {"localOrSyncable", AutofillProfile::RecordType::kLocalOrSyncable},
         {"accountNameEmail", AutofillProfile::RecordType::kAccountNameEmail}});
constexpr std::string_view kKeyInitialCreatorId = "initial_creator_id";

// Checks if the `profile` is changed by `FinalizeAfterImport()`. See
// documentation of `AutofillProfilesFromJSON()` for a rationale.
// The return value of `FinalizeAfterImport()` doesn't suffice to check that,
// since structured address and name components are updated separately.
bool IsFullyStructuredProfile(const AutofillProfile& profile) {
  AutofillProfile finalized_profile = profile;
  finalized_profile.FinalizeAfterImport();
  // TODO(crbug.com/40268162): Re-enable this check.
  // return profile == finalized_profile;
  return true;
}

// Extracts the `kKeyRecordType` value of the `dict` and translates it into an
// AutofillProfile::RecordType. If no value is present,
// RecordType::kLocalOrSyncable is returned. If a record type with invalid value
// is specified, an error message is logged and std::nullopt is returned.
std::optional<AutofillProfile::RecordType> GetRecordTypeFromDict(
    const base::DictValue& dict) {
  if (!dict.contains(kKeyRecordType)) {
    return AutofillProfile::RecordType::kLocalOrSyncable;
  }
  if (const std::string* record_type_value = dict.FindString(kKeyRecordType)) {
    if (auto it = kRecordTypeMapping.find(*record_type_value);
        it != kRecordTypeMapping.end()) {
      return it->second;
    }
  }
  LOG(ERROR) << "Invalid " << kKeyRecordType << " value.";
  return std::nullopt;
}

// Given a `dict` of "field-type" : "value" mappings, constructs an
// AutofillProfile where each "field-type"  is set to the provided "value".
// All verification statuses are set to `kObserved`. Setting them to
// `kUserVerified` is problematic, since the data model expects that only root
// level (= setting-visible) nodes are user verified.
// If a field type cannot be mapped, or if the resulting profile is not
// `IsFullyStructuredProfile()`, std::nullopt is returned.
std::optional<AutofillProfile> MakeProfile(const base::DictValue& dict) {
  std::optional<AutofillProfile::RecordType> record_type =
      GetRecordTypeFromDict(dict);
  if (!record_type.has_value()) {
    return std::nullopt;
  }
  const std::string* country_code =
      dict.FindString(FieldTypeToStringView(ADDRESS_HOME_COUNTRY));
  AddressCountryCode address_country_code =
      country_code ? AddressCountryCode(*country_code) : AddressCountryCode("");

  AutofillProfile profile(*record_type, address_country_code);
  // `dict` is a dictionary of std::string -> base::Value.
  for (const auto [key, value] : dict) {
    if (key == kKeyRecordType) {
      continue;
    }
    if (key == kKeyInitialCreatorId) {
      if (const std::optional<int> creator_id = dict.FindInt(key)) {
        profile.set_initial_creator_id(*creator_id);
        continue;
      } else {
        LOG(ERROR) << "Incorrect value for " << key << ".";
        return std::nullopt;
      }
    }
    const FieldType type = TypeNameToFieldType(key);
    // For phone numbers, only the PHONE_HOME_WHOLE_NUMBER is stored internally
    // and as a result, setting partial phone number is prohibited.
    if (!IsAddressType(type) ||
        (GroupTypeOfFieldType(type) == FieldTypeGroup::kPhone &&
         type != PHONE_HOME_WHOLE_NUMBER)) {
      LOG(ERROR) << "Invalid address type " << key << ".";
      return std::nullopt;
    }
    profile.SetRawInfoWithVerificationStatus(
        type, base::UTF8ToUTF16(value.GetString()),
        VerificationStatus::kObserved);
  }
  if (!IsFullyStructuredProfile(profile)) {
    LOG(ERROR) << "Some profile is not fully structured.";
    return std::nullopt;
  }
  return profile;
}

std::optional<CreditCard> MakeCard(const base::DictValue& dict) {
  CreditCard card;
  // `dict` is a dictionary of std::string -> base::Value.
  for (const auto [key, value] : dict) {
    if (key == kKeyNickname) {
      card.SetNickname(base::UTF8ToUTF16(value.GetString()));
      continue;
    }
    const FieldType type = TypeNameToFieldType(key);
    if (GroupTypeOfFieldType(type) != FieldTypeGroup::kCreditCard) {
      LOG(ERROR) << "Non-credit card type " << key << ".";
      return std::nullopt;
    }
    card.SetRawInfo(type, base::UTF8ToUTF16(value.GetString()));
  }
  if (!card.IsValid()) {
    LOG(ERROR) << "Some credit card is not valid.";
    return std::nullopt;
  }
  return card;
}

std::optional<EntityInstance> MakeEntity(const base::DictValue& dict) {
  const std::string* entity_type_str = dict.FindString(kKeyEntityType);
  if (!entity_type_str) {
    LOG(ERROR) << "Missing " << kKeyEntityType << ".";
    return std::nullopt;
  }
  std::optional<EntityType> entity_type = StringToEntityType(*entity_type_str);
  if (!entity_type) {
    LOG(ERROR) << "Invalid entity type: " << *entity_type_str << ".";
    return std::nullopt;
  }

  const base::DictValue* attributes_dict = dict.FindDict(kKeyAttributes);
  if (!attributes_dict) {
    LOG(ERROR) << "Missing " << kKeyAttributes << ".";
    return std::nullopt;
  }

  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes;
  for (const auto [attr_name, attr_value] : *attributes_dict) {
    std::optional<AttributeType> attribute_type =
        StringToAttributeType(*entity_type, attr_name);
    if (!attribute_type) {
      LOG(ERROR) << "Invalid attribute type: " << attr_name << " for entity "
                 << *entity_type_str << ".";
      return std::nullopt;
    }
    AttributeInstance attribute(*attribute_type);
    attribute.SetRawInfo(std::nullopt,
                         base::UTF8ToUTF16(attr_value.GetString()),
                         VerificationStatus::kObserved);
    attribute.FinalizeInfo();
    attributes.insert(std::move(attribute));
  }

  if (attributes.empty()) {
    LOG(ERROR) << "Entity has no attributes.";
    return std::nullopt;
  }

  return EntityInstance(
      *entity_type, std::move(attributes),
      EntityInstance::EntityId(base::Uuid::GenerateRandomV4()),
      /*nickname=*/"", base::Time::Now(), /*use_count=*/0,
      /*use_date=*/base::Time(), EntityInstance::RecordType::kLocal,
      EntityInstance::AreAttributesReadOnly(false),
      /*frecency_override=*/"");
}

// Removes all AutofillProfiles from the `adm`. Since `ADM::RemoveProfile()`
// invalidates the pointers returned by `ADM::GetProfiles()`, this is done by
// collecting all GUIDs to remove first.
void RemoveAllExistingProfiles(AddressDataManager& adm) {
  std::vector<std::string> existing_guids =
      base::ToVector(adm.GetProfiles(), &AutofillProfile::guid);
  for (const std::string& guid : existing_guids) {
    adm.RemoveProfile(guid);
  }
}

// Sets all of the `pdm`'s profiles or credit cards to `profiles` or
// `credit_cards`, if the `pdm` still exists.
void SetDataForPDM(base::WeakPtr<PersonalDataManager> pdm,
                   std::optional<AutofillImportData> import_data) {
  if (!import_data.has_value() || !import_data->profiles.has_value() ||
      !import_data->credit_cards.has_value()) {
    return;
  }
  if (pdm == nullptr) {
    return;
  }
  // If a list in `import_data` is empty, do not trigger the PDM
  // because this will clear all corresponding existing data.
  if (!import_data->profiles->empty()) {
    RemoveAllExistingProfiles(pdm->address_data_manager());
    for (const AutofillProfile& profile : *import_data->profiles) {
      pdm->address_data_manager().AddProfile(profile);
    }
  }
  if (!import_data->credit_cards->empty()) {
    pdm->payments_data_manager().SetCreditCards(&*import_data->credit_cards);
  }
}

// Sets all of the `edm`'s entities if the `edm` still exists.
void SetDataForEDM(base::WeakPtr<EntityDataManager> edm,
                   std::optional<AutofillImportData> import_data) {
  if (!import_data.has_value() || !import_data->entities.has_value()) {
    return;
  }
  if (edm == nullptr) {
    return;
  }
  if (!import_data->entities->empty()) {
    for (const EntityInstance& entity : *import_data->entities) {
      edm->AddOrUpdateEntityInstance(entity);
    }
  }
}

// Converts all `entries of `json_array` to a vector of Ts using
// `to_data_model`. In case any conversion fails, nullopt is returned.
template <class T>
std::optional<std::vector<T>> DataModelsFromJSON(
    const base::ListValue* const json_array,
    base::RepeatingCallback<std::optional<T>(const base::DictValue&)>
        to_data_model) {
  if (!json_array) {
    return std::vector<T>{};
  }
  std::vector<T> data_models;
  for (const base::Value& json : *json_array) {
    if (!json.is_dict()) {
      LOG(ERROR) << "Description is not a dictionary.";
      return std::nullopt;
    }
    std::optional<T> data_model = to_data_model.Run(json.GetDict());
    if (!data_model.has_value()) {
      return std::nullopt;
    }
    data_models.push_back(std::move(*data_model));
  }
  // Move due to implicit type conversion.
  return std::move(data_models);
}

// Parses Autofill data from the JSON `content` string.
// If parsing fails the error is logged and std::nullopt is returned.
std::optional<AutofillImportData> LoadDataFromJSONContent(
    const std::string& file_content) {
  std::optional<base::DictValue> json = base::JSONReader::ReadDict(
      file_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!json) {
    LOG(ERROR) << "Failed to parse JSON file.";
    return std::nullopt;
  }
  const base::ListValue* const profiles_json = json->FindList(kKeyProfiles);
  const base::ListValue* const cards_json = json->FindList(kKeyCreditCards);
  const base::ListValue* const entities_json = json->FindList(kKeyEntities);
  if (!cards_json && !profiles_json && !entities_json) {
    LOG(ERROR) << "JSON has no " << kKeyProfiles << ", " << kKeyCreditCards
               << " or " << kKeyEntities << " keys.";
    return std::nullopt;
  }
  return AutofillImportData{.profiles = AutofillProfilesFromJSON(profiles_json),
                            .credit_cards = CreditCardsFromJSON(cards_json),
                            .entities = EntitiesFromJSON(entities_json)};
}

std::optional<AutofillImportData> LoadDataFromFile(base::FilePath file) {
  std::string file_content;
  if (!base::ReadFileToString(file, &file_content)) {
    LOG(ERROR) << "Failed to read file " << file.MaybeAsASCII() << ".";
    return std::nullopt;
  }
  return LoadDataFromJSONContent(file_content);
}

}  // namespace

std::optional<std::vector<AutofillProfile>> LoadProfilesFromFile(
    base::FilePath file) {
  if (std::optional<AutofillImportData> import_data = LoadDataFromFile(file)) {
    return import_data->profiles;
  }
  return std::nullopt;
}

std::optional<std::vector<CreditCard>> LoadCreditCardsFromFile(
    base::FilePath file) {
  if (std::optional<AutofillImportData> import_data = LoadDataFromFile(file)) {
    return import_data->credit_cards;
  }
  return std::nullopt;
}

std::optional<std::vector<EntityInstance>> LoadEntitiesFromFile(
    base::FilePath file) {
  if (std::optional<AutofillImportData> import_data = LoadDataFromFile(file)) {
    return import_data->entities;
  }
  return std::nullopt;
}

std::optional<std::vector<AutofillProfile>> AutofillProfilesFromJSON(
    const base::ListValue* const profiles_json) {
  return DataModelsFromJSON(profiles_json, base::BindRepeating(&MakeProfile));
}

std::optional<std::vector<CreditCard>> CreditCardsFromJSON(
    const base::ListValue* const cards_json) {
  return DataModelsFromJSON(cards_json, base::BindRepeating(&MakeCard));
}

std::optional<std::vector<EntityInstance>> EntitiesFromJSON(
    const base::ListValue* const entities_json) {
  return DataModelsFromJSON(entities_json, base::BindRepeating(&MakeEntity));
}

void MaybeImportProfilesAndCardsForTesting(
    base::WeakPtr<PersonalDataManager> pdm) {
  const auto* kCommandLine = base::CommandLine::ForCurrentProcess();
  if (kCommandLine->HasSwitch(kManualFileImportForTestingFlag)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&LoadDataFromFile, kCommandLine->GetSwitchValuePath(
                                              kManualFileImportForTestingFlag)),
        base::BindOnce(&SetDataForPDM, pdm));
  } else if (kCommandLine->HasSwitch(kManualContentImportForTestingFlag)) {
    SetDataForPDM(pdm,
                  LoadDataFromJSONContent(kCommandLine->GetSwitchValueASCII(
                      kManualContentImportForTestingFlag)));
  }
}

void MaybeImportEntitiesForTesting(base::WeakPtr<EntityDataManager> edm) {
  const auto* kCommandLine = base::CommandLine::ForCurrentProcess();
  if (kCommandLine->HasSwitch(kManualFileImportForTestingFlag)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&LoadDataFromFile, kCommandLine->GetSwitchValuePath(
                                              kManualFileImportForTestingFlag)),
        base::BindOnce(&SetDataForEDM, edm));
  } else if (kCommandLine->HasSwitch(kManualContentImportForTestingFlag)) {
    SetDataForEDM(edm,
                  LoadDataFromJSONContent(kCommandLine->GetSwitchValueASCII(
                      kManualContentImportForTestingFlag)));
  }
}

}  // namespace autofill
