// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/manual_testing_profile_import.h"

#include <string>
#include <type_traits>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

constexpr base::StringPiece kKeyProfiles = "profiles";
constexpr base::StringPiece kKeySource = "source";
constexpr auto kSourceMapping =
    base::MakeFixedFlatMap<base::StringPiece, AutofillProfile::Source>(
        {{"account", AutofillProfile::Source::kAccount},
         {"localOrSyncable", AutofillProfile::Source::kLocalOrSyncable}});

using FieldTypeLookupTable = base::flat_map<std::string, ServerFieldType>;

// Builds a mapping from ServerFieldType's string representation to their
// enum type. E.g, "NAME_FULL" -> NAME_FULL. Only meaningful types are
// considered.
FieldTypeLookupTable MakeFieldTypeLookupTable() {
  std::vector<std::pair<std::string, ServerFieldType>> mapping;
  mapping.reserve(MAX_VALID_FIELD_TYPE - NAME_FIRST + 1);
  // NAME_FIRST is the first meaningful type.
  for (std::underlying_type_t<ServerFieldType> type_id = NAME_FIRST;
       type_id <= MAX_VALID_FIELD_TYPE; type_id++) {
    ServerFieldType type = ToSafeServerFieldType(type_id, UNKNOWN_TYPE);
    if (type != UNKNOWN_TYPE) {
      mapping.emplace_back(std::string(FieldTypeToStringPiece(type)), type);
    }
  }
  return FieldTypeLookupTable(std::move(mapping));
}

// Checks if the `profile` is changed by `FinalizeAfterImport()`. See
// documentation of `AutofillProfilesFromJSON()` for a rationale.
// The return value of `FinalizeAfterImport()` doesn't suffice to check that,
// since structured address and name components are updated separately.
bool IsFullyStructuredProfile(const AutofillProfile& profile) {
  AutofillProfile finalized_profile = profile;
  finalized_profile.FinalizeAfterImport();
  return profile == finalized_profile;
}

// Extracts the `kKeySource` value of the `dict` and translates it into an
// AutofillProfile::Source. If no source is present, Source::kLocalOrSyncable is
// returned. If a source with invalid value is specified, nullopt is returned.
absl::optional<AutofillProfile::Source> GetProfileSourceFromDict(
    const base::Value::Dict& dict) {
  if (!dict.contains(kKeySource)) {
    return AutofillProfile::Source::kLocalOrSyncable;
  }
  if (const std::string* source_value = dict.FindString(kKeySource)) {
    auto* it = kSourceMapping.find(*source_value);
    return it != kSourceMapping.end() ? absl::make_optional(it->second)
                                      : absl::nullopt;
  }
  return absl::nullopt;
}

// Given a `dict` of "field-type" : "value" mappings, constructs an
// AutofillProfile where each "field-type"  is set to the provided "value".
// "field-type"s are converted to ServerFieldTypes using the `lookup_table`.
// All verification statuses are set to `kUserVerified`.
// If a field type cannot be mapped, or if the resulting profile is not
// `IsFullyStructuredProfile()`, nullopt is returned.
absl::optional<AutofillProfile> MakeProfile(
    const base::Value::Dict& dict,
    const FieldTypeLookupTable& lookup_table) {
  absl::optional<AutofillProfile::Source> source =
      GetProfileSourceFromDict(dict);
  if (!source) {
    return absl::nullopt;
  }
  AutofillProfile profile(*source);
  // `dict` is a dictionary of std::string -> base::Value.
  for (const auto [key, value] : dict) {
    if (key == kKeySource) {
      continue;
    }
    if (!lookup_table.contains(key)) {
      return absl::nullopt;
    }
    profile.SetRawInfoWithVerificationStatus(
        lookup_table.at(key), base::UTF8ToUTF16(value.GetString()),
        VerificationStatus::kUserVerified);
  }
  if (!IsFullyStructuredProfile(profile)) {
    return absl::nullopt;
  }
  return profile;
}

// Reads the contents of `file`, parses it as a JSON file and converts its
// content into AutofillProfiles.
// To prevent testers from working with invalid data, the function intentionally
// crashes if the file cannot be read of parsed.
std::vector<AutofillProfile> LoadProfilesFromFile(base::FilePath file) {
  std::string file_content;
  CHECK(base::ReadFileToString(file, &file_content))
      << "Failed to read file " << file.value();
  absl::optional<base::Value> json = base::JSONReader::Read(file_content);
  CHECK(json) << "Failed to parse JSON";
  absl::optional<std::vector<AutofillProfile>> profiles =
      AutofillProfilesFromJSON(*json);
  CHECK(profiles) << "Failed to convert JSON to AutofillProfiles";
  return *profiles;
}

// Sets all of the `pdm`'s profiles to `profiles`, if the `pdm` still exists.
void SetProfiles(base::WeakPtr<PersonalDataManager> pdm,
                 std::vector<AutofillProfile> profiles) {
  if (pdm) {
    pdm->SetProfilesForAllSources(&profiles);
  }
}

}  // namespace

absl::optional<std::vector<AutofillProfile>> AutofillProfilesFromJSON(
    const base::Value& json) {
  if (!json.is_dict()) {
    return absl::nullopt;
  }
  const base::Value::List* profiles_json =
      json.GetDict().FindList(kKeyProfiles);
  if (!profiles_json) {
    return absl::nullopt;
  }

  const auto kLookupTable = MakeFieldTypeLookupTable();
  std::vector<AutofillProfile> profiles_to_import;
  for (const base::Value& profile_json : *profiles_json) {
    if (!profile_json.is_dict()) {
      return absl::nullopt;
    }
    if (auto profile = MakeProfile(profile_json.GetDict(), kLookupTable)) {
      profiles_to_import.push_back(*profile);
    } else {
      return absl::nullopt;
    }
  }
  return profiles_to_import;
}

void MaybeImportProfilesForManualTesting(
    base::WeakPtr<PersonalDataManager> pdm) {
  const auto* kCommandLine = base::CommandLine::ForCurrentProcess();
  if (kCommandLine->HasSwitch(kManualProfileImportForTestingFlag)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&LoadProfilesFromFile,
                       kCommandLine->GetSwitchValuePath(
                           kManualProfileImportForTestingFlag)),
        base::BindOnce(&SetProfiles, pdm));
  }
}

}  // namespace autofill
