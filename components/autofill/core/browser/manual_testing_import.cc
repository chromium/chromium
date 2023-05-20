// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/manual_testing_import.h"

#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

// Util struct for storing the list of profiles and credit cards to be imported.
// If any of `profiles` or `credit_cards` are absl::nullopt, then the data used
// for import is malformed, and this will cause a crash.
// When any of `profiles` or `credit_cards` is empty, it means that the JSON
// file used for import did not include the corresponding key. It'll be treated
// as valid but won't be imported so that existing data in the PDM isn't
// cleared without replacement.
struct AutofillProfilesAndCreditCards {
  absl::optional<std::vector<AutofillProfile>> profiles;
  absl::optional<std::vector<CreditCard>> credit_cards;
};

constexpr base::StringPiece kKeyProfiles = "profiles";
constexpr base::StringPiece kKeyCreditCards = "credit-cards";
constexpr base::StringPiece kKeySource = "source";
constexpr base::StringPiece kKeyNickname = "nickname";
constexpr auto kSourceMapping =
    base::MakeFixedFlatMap<base::StringPiece, AutofillProfile::Source>(
        {{"account", AutofillProfile::Source::kAccount},
         {"localOrSyncable", AutofillProfile::Source::kLocalOrSyncable}});
constexpr base::StringPiece kKeyInitialCreatorId = "initial_creator_id";

// Checks if the `profile` is changed by `FinalizeAfterImport()`. See
// documentation of `AutofillProfilesFromJSON()` for a rationale.
// The return value of `FinalizeAfterImport()` doesn't suffice to check that,
// since structured address and name components are updated separately.
bool IsFullyStructuredProfile(const AutofillProfile& profile) {
  AutofillProfile finalized_profile = profile;
  finalized_profile.FinalizeAfterImport();
  // TODO(1445454): Re-enable this check.
  // return profile == finalized_profile;
  return true;
}

// Extracts the `kKeySource` value of the `dict` and translates it into an
// AutofillProfile::Source. If no source is present, Source::kLocalOrSyncable is
// returned. If a source with invalid value is specified, an error message is
// logged and absl::nullopt is returned.
absl::optional<AutofillProfile::Source> GetProfileSourceFromDict(
    const base::Value::Dict& dict) {
  if (!dict.contains(kKeySource)) {
    return AutofillProfile::Source::kLocalOrSyncable;
  }
  if (const std::string* source_value = dict.FindString(kKeySource)) {
    if (auto* it = kSourceMapping.find(*source_value);
        it != kSourceMapping.end()) {
      return it->second;
    }
  }
  LOG(ERROR) << "Invalid " << kKeySource << " value.";
  return absl::nullopt;
}

// Given a `dict` of "field-type" : "value" mappings, constructs an
// AutofillProfile where each "field-type"  is set to the provided "value".
// All verification statuses are set to `kObserved`. Setting them to
// `kUserVerified` is problematic, since the data model expects that only root
// level (= setting-visible) nodes are user verified.
// If a field type cannot be mapped, or if the resulting profile is not
// `IsFullyStructuredProfile()`, absl::nullopt is returned.
absl::optional<AutofillProfile> MakeProfile(const base::Value::Dict& dict) {
  absl::optional<AutofillProfile::Source> source =
      GetProfileSourceFromDict(dict);
  if (!source.has_value()) {
    return absl::nullopt;
  }
  AutofillProfile profile(*source);
  // `dict` is a dictionary of std::string -> base::Value.
  for (const auto [key, value] : dict) {
    if (key == kKeySource) {
      continue;
    }
    if (key == kKeyInitialCreatorId) {
      if (const absl::optional<int> creator_id = dict.FindInt(key)) {
        profile.set_initial_creator_id(*creator_id);
        continue;
      } else {
        LOG(ERROR) << "Incorrect value for " << key << ".";
        return absl::nullopt;
      }
    }
    const ServerFieldType type = TypeNameToFieldType(key);
    if (type == UNKNOWN_TYPE || !IsAddressType(AutofillType(type))) {
      LOG(ERROR) << "Unknown or non-address type " << key << ".";
      return absl::nullopt;
    }
    profile.SetRawInfoWithVerificationStatus(
        type, base::UTF8ToUTF16(value.GetString()),
        VerificationStatus::kObserved);
  }
  if (!IsFullyStructuredProfile(profile)) {
    LOG(ERROR) << "Some profile is not fully structured.";
    return absl::nullopt;
  }
  return profile;
}

absl::optional<CreditCard> MakeCard(const base::Value::Dict& dict) {
  CreditCard card;
  // `dict` is a dictionary of std::string -> base::Value.
  for (const auto [key, value] : dict) {
    if (key == kKeyNickname) {
      card.SetNickname(base::UTF8ToUTF16(value.GetString()));
      continue;
    }
    const ServerFieldType type = TypeNameToFieldType(key);
    if (type == UNKNOWN_TYPE ||
        GroupTypeOfServerFieldType(type) != FieldTypeGroup::kCreditCard) {
      LOG(ERROR) << "Unknown or non-credit card type " << key << ".";
      return absl::nullopt;
    }
    card.SetRawInfo(type, base::UTF8ToUTF16(value.GetString()));
  }
  if (!card.IsValid()) {
    LOG(ERROR) << "Some credit card is not valid.";
    return absl::nullopt;
  }
  return card;
}

// Sets all of the `pdm`'s profiles or credit cards to `profiles` or
// `credit_cards`, if the `pdm` still exists.
void SetData(
    base::WeakPtr<PersonalDataManager> pdm,
    absl::optional<AutofillProfilesAndCreditCards> profiles_or_credit_cards) {
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
    pdm->SetProfilesForAllSources(&*profiles_or_credit_cards->profiles);
  }
  if (!profiles_or_credit_cards->credit_cards->empty()) {
    pdm->SetCreditCards(&*profiles_or_credit_cards->credit_cards);
  }
}

// Converts all `entries of `json_array` to a vector of Ts using
// `to_data_model`. In case any conversion fails, nullopt is returned.
template <class T>
absl::optional<std::vector<T>> DataModelsFromJSON(
    const base::Value::List* const json_array,
    base::RepeatingCallback<absl::optional<T>(const base::Value::Dict&)>
        to_data_model) {
  if (!json_array) {
    return std::vector<T>{};
  }
  std::vector<T> data_models;
  for (const base::Value& json : *json_array) {
    if (!json.is_dict()) {
      LOG(ERROR) << "Description is not a dictionary.";
      return absl::nullopt;
    }
    absl::optional<T> data_model = to_data_model.Run(json.GetDict());
    if (!data_model.has_value()) {
      return absl::nullopt;
    }
    data_models.push_back(std::move(*data_model));
  }
  // Move due to implicit type conversion.
  return std::move(data_models);
}

// Parses AutofillProfiles from the JSON `content` string.
// If parsing fails the error is logged and absl::nullopt is returned.
absl::optional<AutofillProfilesAndCreditCards> LoadDataFromJSONContent(
    const std::string& file_content) {
  absl::optional<base::Value> json = base::JSONReader::Read(file_content);
  if (!json.has_value()) {
    LOG(ERROR) << "Failed to parse JSON file.";
    return absl::nullopt;
  }
  if (!json->is_dict()) {
    LOG(ERROR) << "JSON is not a dictionary at it's top level.";
    return absl::nullopt;
  }
  const base::Value::List* const profiles_json =
      json->GetDict().FindList(kKeyProfiles);
  const base::Value::List* const cards_json =
      json->GetDict().FindList(kKeyCreditCards);
  if (!cards_json && !profiles_json) {
    LOG(ERROR) << "JSON has no " << kKeyProfiles << " or " << kKeyCreditCards
               << " keys.";
    return absl::nullopt;
  }
  return AutofillProfilesAndCreditCards{
      .profiles = AutofillProfilesFromJSON(profiles_json),
      .credit_cards = CreditCardsFromJSON(cards_json)};
}

absl::optional<AutofillProfilesAndCreditCards> LoadDataFromFile(
    base::FilePath file) {
  std::string file_content;
  if (!base::ReadFileToString(file, &file_content)) {
    LOG(ERROR) << "Failed to read file " << file.MaybeAsASCII() << ".";
    return absl::nullopt;
  }
  return LoadDataFromJSONContent(file_content);
}

}  // namespace

absl::optional<std::vector<AutofillProfile>> LoadProfilesFromFile(
    base::FilePath file) {
  if (absl::optional<AutofillProfilesAndCreditCards> profiles_and_credit_cards =
          LoadDataFromFile(file)) {
    return profiles_and_credit_cards->profiles;
  }
  return absl::nullopt;
}

absl::optional<std::vector<CreditCard>> LoadCreditCardsFromFile(
    base::FilePath file) {
  if (absl::optional<AutofillProfilesAndCreditCards> profiles_and_credit_cards =
          LoadDataFromFile(file)) {
    return profiles_and_credit_cards->credit_cards;
  }
  return absl::nullopt;
}

absl::optional<std::vector<AutofillProfile>> AutofillProfilesFromJSON(
    const base::Value::List* const profiles_json) {
  return DataModelsFromJSON(profiles_json, base::BindRepeating(&MakeProfile));
}

absl::optional<std::vector<CreditCard>> CreditCardsFromJSON(
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
