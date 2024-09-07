// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/manual_testing_import.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments_data_manager.h"

namespace autofill {

namespace {

// Util struct for storing the list of profiles and credit cards to be imported.
// If any of `profiles` or `credit_cards` are std::nullopt, then the data used
// for import is malformed, and this will cause a crash.
// When any of `profiles` or `credit_cards` is empty, it means that the JSON
// file used for import did not include the corresponding key. It'll be treated
// as valid but won't be imported so that existing data in the PDM isn't
// cleared without replacement.
struct AutofillProfilesAndCreditCards {
  std::optional<std::vector<AutofillProfile>> profiles;
  std::optional<std::vector<CreditCard>> credit_cards;
};

constexpr std::string_view kKeyProfiles = "profiles";
constexpr std::string_view kKeyCreditCards = "credit-cards";
constexpr std::string_view kKeyRecordType = "record_type";
constexpr std::string_view kKeyNickname = "nickname";
constexpr auto kRecordTypeMapping =
    base::MakeFixedFlatMap<std::string_view, AutofillProfile::RecordType>(
        {{"account", AutofillProfile::RecordType::kAccount},
         {"localOrSyncable", AutofillProfile::RecordType::kLocalOrSyncable}});
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
    const base::Value::Dict& dict) {
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
std::optional<AutofillProfile> MakeProfile(const base::Value::Dict& dict) {
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
    if (type == UNKNOWN_TYPE || !IsAddressType(type)) {
      LOG(ERROR) << "Unknown or non-address type " << key << ".";
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

std::optional<CreditCard> MakeCard(const base::Value::Dict& dict) {
  CreditCard card;
  // `dict` is a dictionary of std::string -> base::Value.
  for (const auto [key, value] : dict) {
    if (key == kKeyNickname) {
      card.SetNickname(base::UTF8ToUTF16(value.GetString()));
      continue;
    }
    const FieldType type = TypeNameToFieldType(key);
    if (type == UNKNOWN_TYPE ||
        GroupTypeOfFieldType(type) != FieldTypeGroup::kCreditCard) {
      LOG(ERROR) << "Unknown or non-credit card type " << key << ".";
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

// Removes all AutofillProfiles from the `pdm`. Since `PDM::RemoveByGUID()`
// invalidates the pointers returned by `PDM::GetProfiles()`, this is done by
// collecting all GUIDs to remove first.
void RemoveAllExistingProfiles(PersonalDataManager& pdm) {
  std::vector<std::string> existing_guids;
  base::ranges::transform(pdm.address_data_manager().GetProfiles(),
                          std::back_inserter(existing_guids),
                          &AutofillProfile::guid);
  for (const std::string& guid : existing_guids) {
    pdm.RemoveByGUID(guid);
  }
}

// Sets all of the `pdm`'s profiles or credit cards to `profiles` or
// `credit_cards`, if the `pdm` still exists.
void SetData(
    base::WeakPtr<PersonalDataManager> pdm,
    std::optional<AutofillProfilesAndCreditCards> profiles_or_credit_cards) {
  // This check intentionally crashes when the data is malformed, to prevent
  // testing with incorrect data.
  LOG_IF(FATAL, !profiles_or_credit_cards.has_value() ||
                    !profiles_or_credit_cards->profiles.has_value() ||
                    !profiles_or_credit_cards->credit_cards.has_value())
      << "Intentional crash, the provided JSON import data is incorrect.";
  if (pdm == nullptr) {
    return;
  }
  // If a list in `profiles_or_credit_cards` is empty, do not trigger the PDM
  // because this will clear all corresponding existing data.
  if (!profiles_or_credit_cards->profiles->empty()) {
    RemoveAllExistingProfiles(*pdm);
    for (const AutofillProfile& profile : *profiles_or_credit_cards->profiles) {
      pdm->address_data_manager().AddProfile(profile);
    }
  }
  if (!profiles_or_credit_cards->credit_cards->empty()) {
    pdm->payments_data_manager().SetCreditCards(
        &*profiles_or_credit_cards->credit_cards);
  }
}

// Converts all `entries of `json_array` to a vector of Ts using
// `to_data_model`. In case any conversion fails, nullopt is returned.
template <class T>
std::optional<std::vector<T>> DataModelsFromJSON(
    const base::Value::List* const json_array,
    base::RepeatingCallback<std::optional<T>(const base::Value::Dict&)>
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

// Parses AutofillProfiles from the JSON `content` string.
// If parsing fails the error is logged and std::nullopt is returned.
std::optional<AutofillProfilesAndCreditCards> LoadDataFromJSONContent(
    const std::string& file_content) {
  std::optional<base::Value> json = base::JSONReader::Read(file_content);
  if (!json.has_value()) {
    LOG(ERROR) << "Failed to parse JSON file.";
    return std::nullopt;
  }
  if (!json->is_dict()) {
    LOG(ERROR) << "JSON is not a dictionary at it's top level.";
    return std::nullopt;
  }
  const base::Value::List* const profiles_json =
      json->GetDict().FindList(kKeyProfiles);
  const base::Value::List* const cards_json =
      json->GetDict().FindList(kKeyCreditCards);
  if (!cards_json && !profiles_json) {
    LOG(ERROR) << "JSON has no " << kKeyProfiles << " or " << kKeyCreditCards
               << " keys.";
    return std::nullopt;
  }
  return AutofillProfilesAndCreditCards{
      .profiles = AutofillProfilesFromJSON(profiles_json),
      .credit_cards = CreditCardsFromJSON(cards_json)};
}

std::optional<AutofillProfilesAndCreditCards> LoadDataFromFile(
    base::FilePath file) {
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
  if (std::optional<AutofillProfilesAndCreditCards> profiles_and_credit_cards =
          LoadDataFromFile(file)) {
    return profiles_and_credit_cards->profiles;
  }
  return std::nullopt;
}

std::optional<std::vector<CreditCard>> LoadCreditCardsFromFile(
    base::FilePath file) {
  if (std::optional<AutofillProfilesAndCreditCards> profiles_and_credit_cards =
          LoadDataFromFile(file)) {
    return profiles_and_credit_cards->credit_cards;
  }
  return std::nullopt;
}

std::optional<std::vector<AutofillProfile>> AutofillProfilesFromJSON(
    const base::Value::List* const profiles_json) {
  return DataModelsFromJSON(profiles_json, base::BindRepeating(&MakeProfile));
}

std::optional<std::vector<CreditCard>> CreditCardsFromJSON(
    const base::Value::List* const cards_json) {
  return DataModelsFromJSON(cards_json, base::BindRepeating(&MakeCard));
}

void MaybeImportDataForManualTesting(base::WeakPtr<PersonalDataManager> pdm) {
  const auto* kCommandLine = base::CommandLine::ForCurrentProcess();
  if (kCommandLine->HasSwitch(kManualFileImportForTestingFlag)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&LoadDataFromFile, kCommandLine->GetSwitchValuePath(
                                              kManualFileImportForTestingFlag)),
        base::BindOnce(&SetData, pdm));
  } else if (kCommandLine->HasSwitch(kManualContentImportForTestingFlag)) {
    SetData(pdm, LoadDataFromJSONContent(kCommandLine->GetSwitchValueASCII(
                     kManualContentImportForTestingFlag)));
  }
}

}  // namespace autofill
