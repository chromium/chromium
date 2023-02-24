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
#include "base/strings/strcat.h"
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
// returned. If a source with invalid value is specified, an error message is
// returned.
base::expected<AutofillProfile::Source, std::string> GetProfileSourceFromDict(
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
  return base::unexpected(base::StrCat({"Invalid ", kKeySource, " value"}));
}

// Given a `dict` of "field-type" : "value" mappings, constructs an
// AutofillProfile where each "field-type"  is set to the provided "value".
// "field-type"s are converted to ServerFieldTypes using the `lookup_table`.
// All verification statuses are set to `kUserVerified`.
// If a field type cannot be mapped, or if the resulting profile is not
// `IsFullyStructuredProfile()`, nullopt is returned.
base::expected<AutofillProfile, std::string> MakeProfile(
    const base::Value::Dict& dict,
    const FieldTypeLookupTable& lookup_table) {
  base::expected<AutofillProfile::Source, std::string> source =
      GetProfileSourceFromDict(dict);
  if (!source.has_value()) {
    return base::unexpected(source.error());
  }
  AutofillProfile profile(*source);
  // `dict` is a dictionary of std::string -> base::Value.
  for (const auto [key, value] : dict) {
    if (key == kKeySource) {
      continue;
    }
    if (!lookup_table.contains(key)) {
      return base::unexpected("Unknown type " + key);
    }
    profile.SetRawInfoWithVerificationStatus(
        lookup_table.at(key), base::UTF8ToUTF16(value.GetString()),
        VerificationStatus::kUserVerified);
  }
  if (!IsFullyStructuredProfile(profile)) {
    return base::unexpected("Not a fully structured profile");
  }
  return profile;
}

// Reads the contents of `file`, parses it as a JSON file and converts its
// content into AutofillProfiles.
// If any step fails, an error message is returned.
base::expected<std::vector<AutofillProfile>, std::string> LoadProfilesFromFile(
    base::FilePath file) {
  std::string file_content;
  if (!base::ReadFileToString(file, &file_content)) {
    return base::unexpected("Failed to read file " + file.MaybeAsASCII());
  }
  if (absl::optional<base::Value> json = base::JSONReader::Read(file_content)) {
    return AutofillProfilesFromJSON(*json);
  }
  return base::unexpected("Failed to parse JSON");
}

// Sets all of the `pdm`'s profiles to `profiles`, if the `pdm` still exists.
void SetProfiles(
    base::WeakPtr<PersonalDataManager> pdm,
    base::expected<std::vector<AutofillProfile>, std::string> profiles) {
  CHECK(profiles.has_value()) << profiles.error();
  if (pdm) {
    pdm->SetProfilesForAllSources(&*profiles);
  }
}

}  // namespace

base::expected<std::vector<AutofillProfile>, std::string>
AutofillProfilesFromJSON(const base::Value& json) {
  if (!json.is_dict()) {
    return base::unexpected("JSON is not a dictionary at it's top level");
  }
  const base::Value::List* profiles_json =
      json.GetDict().FindList(kKeyProfiles);
  if (!profiles_json) {
    return base::unexpected(base::StrCat({"No ", kKeyProfiles, " key"}));
  }

  const auto kLookupTable = MakeFieldTypeLookupTable();
  std::vector<AutofillProfile> profiles_to_import;
  for (const base::Value& profile_json : *profiles_json) {
    if (!profile_json.is_dict()) {
      return base::unexpected("Profile description to not a dictionary");
    }
    base::expected<AutofillProfile, std::string> profile =
        MakeProfile(profile_json.GetDict(), kLookupTable);
    if (!profile.has_value()) {
      return base::unexpected(profile.error());
    }
    profiles_to_import.push_back(*profile);
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
