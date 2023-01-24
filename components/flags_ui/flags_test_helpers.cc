// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/flags_test_helpers.h"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_state.h"

namespace {

// Type of flag ownership file.
enum class FlagFile { kFlagMetadata, kFlagNeverExpire };

// Returns the filename based on the file enum.
std::string FlagFileName(FlagFile file) {
  switch (file) {
    case FlagFile::kFlagMetadata:
      return "flag-metadata.json";
    case FlagFile::kFlagNeverExpire:
      return "flag-never-expire-list.json";
  }
}

// Returns the JSON file contents.
base::Value FileContents(FlagFile file) {
  std::string filename = FlagFileName(file);

  base::FilePath metadata_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &metadata_path);
  JSONFileValueDeserializer deserializer(
      metadata_path.AppendASCII("chrome").AppendASCII("browser").AppendASCII(
          filename));
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> json =
      deserializer.Deserialize(&error_code, &error_message);
  DCHECK(json) << "Failed to load " << filename << ": " << error_code << " "
               << error_message;
  return std::move(*json);
}

// Data structure capturing the metadata of the flag.
struct FlagMetadataEntry {
  std::vector<std::string> owners;
  int expiry_milestone;
};

// Lookup of metadata by flag name.
using FlagMetadataMap = std::map<std::string, FlagMetadataEntry>;

// Reads the flag metadata file.
FlagMetadataMap LoadFlagMetadata() {
  base::Value metadata_json = FileContents(FlagFile::kFlagMetadata);

  FlagMetadataMap metadata;
  for (const auto& entry_val : metadata_json.GetList()) {
    const base::Value::Dict& entry = entry_val.GetDict();
    std::string name = *entry.FindString("name");
    std::vector<std::string> owners;
    if (const base::Value::List* e = entry.FindList("owners")) {
      for (const auto& owner : *e) {
        owners.push_back(owner.GetString());
      }
    }
    int expiry_milestone = entry.FindInt("expiry_milestone").value();
    metadata[name] = FlagMetadataEntry{owners, expiry_milestone};
  }

  return metadata;
}

std::vector<std::string> LoadFlagNeverExpireList() {
  base::Value list_json = FileContents(FlagFile::kFlagNeverExpire);

  std::vector<std::string> result;
  for (const auto& entry : list_json.GetList()) {
    result.push_back(entry.GetString());
  }
  return result;
}

bool IsValidLookingOwner(base::StringPiece owner) {
  // Never allow ',' or ' ' in owner names, regardless of all other constraints.
  // It is otherwise too easy to accidentally do this:
  //   "owners": [ "foo@chromium.org,bar@chromium.org" ]
  // or this:
  //   "owners": [ "foo@chromium.org bar@chromium.org" ]
  // Apologies to those who have spaces in their email addresses or OWNERS file
  // path names :)
  if (owner.find_first_of(", ") != std::string::npos)
    return false;

  // Per the specification at the top of flag-metadata.json, an owner is one of:
  // 1) A string containing '@', which is treated as a full email address
  // 2) A string beginning with '//', which is a path to an OWNERS file
  // 3) Any other string, which is the username part of an @chromium.org email

  const size_t at_pos = owner.find("@");
  if (at_pos != std::string::npos) {
    // If there's an @, check for a . after it. This catches one common error:
    // writing "foo@" in the owners list rather than bare "foo" or full
    // "foo@domain.com".
    return owner.find(".", at_pos) != std::string::npos;
  }

  if (base::StartsWith(owner, "//")) {
    // Looks like a path to a file. It would be nice to check that the file
    // actually exists here, but that's not possible because when this test
    // runs it runs in an isolated environment. To check for the presence of the
    // file the test would need a build-time declaration that it depends on that
    // file. Instead, just assume any file path ending in 'OWNERS' is valid.
    // This doesn't check that the entire filename part of the path is 'OWNERS'
    // because sometimes it is instead 'IPC_OWNERS' or similar.
    return base::EndsWith(owner, "OWNERS");
  }

  // Otherwise, look for something that seems like the username part of an
  // @chromium.org email. The criteria here is that it must look like an RFC5322
  // "atom", which is neatly defined as any printable character *outside* a
  // specific set:
  //   https://tools.ietf.org/html/rfc5322#section-3.2.3
  //
  // Note two extra wrinkles here:
  // 1) while '.' IS NOT allowed in atoms by RFC5322 gmail and other mail
  //    handlers do allow it, so this does not reject '.'.
  // 2) while '/' IS allowed in atoms by RFC5322, this is not commonly done, and
  //    checking for it here detects another common syntax error - namely
  //    writing:
  //      "owners": [ "foo/bar/OWNERS" ]
  //    where
  //      "owners": [ "//foo/bar/OWNERS" ]
  //    is meant.
  return owner.find_first_of(R"(()<>[]:;@\,/)") == std::string::npos;
}

void EnsureNamesAreAlphabetical(
    const std::vector<std::string>& normalized_names,
    const std::vector<std::string>& names,
    FlagFile file) {
  if (normalized_names.size() < 2)
    return;

  for (size_t i = 1; i < normalized_names.size(); ++i) {
    if (i == normalized_names.size() - 1) {
      // The last item on the list has less context.
      EXPECT_TRUE(normalized_names[i - 1] < normalized_names[i])
          << "Correct alphabetical order does not place '" << names[i]
          << "' after '" << names[i - 1] << "' in " << FlagFileName(file);
    } else {
      EXPECT_TRUE(normalized_names[i - 1] < normalized_names[i] &&
                  normalized_names[i] < normalized_names[i + 1])
          << "Correct alphabetical order does not place '" << names[i]
          << "' between '" << names[i - 1] << "' and '" << names[i + 1]
          << "' in " << FlagFileName(file);
    }
  }
}

std::string NormalizeName(const std::string& name) {
  std::string normalized_name = base::ToLowerASCII(name);
  std::replace(normalized_name.begin(), normalized_name.end(), '_', '-');

  return normalized_name;
}

bool IsUnexpireFlagFor(const flags_ui::FeatureEntry& entry, int milestone) {
  std::string expected_flag =
      base::StringPrintf("temporary-unexpire-flags-m%d", milestone);
  if (entry.internal_name != expected_flag)
    return false;
  if (!(entry.supported_platforms & flags_ui::kFlagInfrastructure))
    return false;
  if (entry.type != flags_ui::FeatureEntry::FEATURE_VALUE)
    return false;
  std::string expected_feature =
      base::StringPrintf("UnexpireFlagsM%d", milestone);
  const auto* feature = entry.feature.feature;
  if (!feature || feature->name != expected_feature)
    return false;
  return true;
}

}  // namespace

namespace flags_ui {

namespace testing {

void EnsureEveryFlagHasMetadata(
    const base::span<const flags_ui::FeatureEntry>& entries) {
  FlagMetadataMap metadata = LoadFlagMetadata();
  std::vector<std::string> missing_flags;

  for (const auto& entry : entries) {
    // Flags that are part of the flags system itself (like unexpiry meta-flags)
    // don't have metadata, so skip them here.
    if (entry.supported_platforms & flags_ui::kFlagInfrastructure)
      continue;

    if (metadata.count(entry.internal_name) == 0)
      missing_flags.push_back(entry.internal_name);
  }

  std::sort(missing_flags.begin(), missing_flags.end());

  EXPECT_EQ(0u, missing_flags.size())
      << "Missing flags: " << base::JoinString(missing_flags, "\n  ");
}

void EnsureOnlyPermittedFlagsNeverExpire() {
  FlagMetadataMap metadata = LoadFlagMetadata();
  std::vector<std::string> listed_flags = LoadFlagNeverExpireList();
  std::vector<std::string> missing_flags;

  for (const auto& entry : metadata) {
    if (entry.second.expiry_milestone == -1 &&
        !base::Contains(listed_flags, entry.first)) {
      missing_flags.push_back(entry.first);
    }
  }

  std::sort(missing_flags.begin(), missing_flags.end());

  EXPECT_EQ(0u, missing_flags.size())
      << "Flags not listed for no-expire: "
      << base::JoinString(missing_flags, "\n  ");
}

void EnsureEveryFlagHasNonEmptyOwners() {
  FlagMetadataMap metadata = LoadFlagMetadata();
  std::vector<std::string> sad_flags;

  for (const auto& it : metadata) {
    if (it.second.owners.empty())
      sad_flags.push_back(it.first);
  }

  std::sort(sad_flags.begin(), sad_flags.end());

  EXPECT_EQ(0u, sad_flags.size())
      << "Flags missing owners: " << base::JoinString(sad_flags, "\n  ");
}

void EnsureOwnersLookValid() {
  FlagMetadataMap metadata = LoadFlagMetadata();
  std::vector<std::string> sad_flags;

  for (const auto& flag : metadata) {
    for (const auto& owner : flag.second.owners) {
      if (!IsValidLookingOwner(owner))
        sad_flags.push_back(flag.first);
    }
  }

  EXPECT_EQ(0u, sad_flags.size()) << "Flags with invalid-looking owners: "
                                  << base::JoinString(sad_flags, "\n");
}

void EnsureFlagsAreListedInAlphabeticalOrder() {
  base::Value metadata_json = FileContents(FlagFile::kFlagMetadata);

  std::vector<std::string> normalized_names;
  std::vector<std::string> names;
  for (const auto& entry_val : metadata_json.GetList()) {
    const base::Value::Dict& entry = entry_val.GetDict();
    normalized_names.push_back(NormalizeName(*entry.FindString("name")));
    names.push_back(*entry.FindString("name"));
  }

  EnsureNamesAreAlphabetical(normalized_names, names, FlagFile::kFlagMetadata);

  base::Value expiration_json = FileContents(FlagFile::kFlagNeverExpire);

  normalized_names.clear();
  names.clear();
  for (const auto& entry : expiration_json.GetList()) {
    normalized_names.push_back(NormalizeName(entry.GetString()));
    names.push_back(entry.GetString());
  }

  EnsureNamesAreAlphabetical(normalized_names, names,
                             FlagFile::kFlagNeverExpire);
}

// TODO(https://crbug.com/1241068): Call this from the iOS flags unittests once
// flag expiration is supported there.
void EnsureRecentUnexpireFlagsArePresent(
    const base::span<const flags_ui::FeatureEntry>& entries,
    int current_milestone) {
  auto contains_unexpire_for = [&](int mstone) {
    for (const auto& entry : entries) {
      if (IsUnexpireFlagFor(entry, mstone))
        return true;
    }
    return false;
  };

  EXPECT_FALSE(contains_unexpire_for(current_milestone));
  EXPECT_TRUE(contains_unexpire_for(current_milestone - 1));
  EXPECT_TRUE(contains_unexpire_for(current_milestone - 2));
  EXPECT_FALSE(contains_unexpire_for(current_milestone - 3));
}

}  // namespace testing

}  // namespace flags_ui
