// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/flags_ui/flags_test_helpers.h"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_state.h"

namespace {

constexpr char kMetadataFileName[] = "flag-metadata.json";
constexpr char kNeverExpireFileName[] = "flag-never-expire-list.json";

// Returns the file contents of a named file under $SRC/chrome/browser
// interpreted as a JSON value.
base::Value ReadFileContentsAsJSON(const std::string& filename) {
  base::FilePath metadata_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &metadata_path);
  JSONFileValueDeserializer deserializer(
      metadata_path.AppendASCII("chrome").AppendASCII("browser").AppendASCII(
          filename));
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> json =
      deserializer.Deserialize(&error_code, &error_message);
  CHECK(json) << "Failed to load " << filename << ": " << error_code << " "
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
  base::Value metadata_json = ReadFileContentsAsJSON(kMetadataFileName);

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
  base::Value list_json = ReadFileContentsAsJSON(kNeverExpireFileName);

  std::vector<std::string> result;
  for (const auto& entry : list_json.GetList()) {
    result.push_back(entry.GetString());
  }
  return result;
}

bool IsValidLookingOwner(std::string_view owner) {
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

  return false;
}

std::string NormalizeName(const std::string& name) {
  std::string normalized_name = base::ToLowerASCII(name);
  std::replace(normalized_name.begin(), normalized_name.end(), '_', '-');

  return normalized_name;
}

constexpr char kStartSentinel[] = "(start of file)";

using NameNameMap = std::map<std::string, std::string>;
using NameVector = std::vector<std::string>;

// Given a NameVector, returns a map from each name n to the name preceding n in
// the NameVector. The returned map maps the first name to kStartSentinel.
// Preconditions:
//   * There are no duplicates in |strings|
//   * No entry in |strings| equals kStartSentinel
// Postconditions:
//   * Every entry in |strings| appears as a key in the result map
//   * Every entry in |strings| maps to another entry in |strings| or to
//     kStartSentinel in the result map
NameNameMap BuildAfterMap(const NameVector& strings) {
  NameNameMap after_map;
  CHECK_NE(strings[0], kStartSentinel);
  after_map[strings[0]] = kStartSentinel;
  for (size_t i = 1; i < strings.size(); ++i) {
    CHECK_NE(strings[i], kStartSentinel);
    CHECK(!after_map.contains(strings[i]));
    after_map[strings[i]] = strings[i - 1];
  }

  // Postconditions:
  for (const auto& entry : strings) {
    CHECK(after_map.contains(entry));
  }

  return after_map;
}

// Given a vector of names, returns a vector of normalized names, and an inverse
// mapping from normalized name to previous name. The inverse mapping is
// populated only for names which were altered when normalized.
// Preconditions: none
// Postconditions:
//   * Every (key, value) pair in |denormalized| have key != value
//   * Every (key, value) pair in |denormalized| have key = NormalizeName(value)
std::pair<NameVector, NameNameMap> NormalizeNames(const NameVector& names) {
  NameNameMap denormalized;
  NameVector normalized;
  for (const auto& name : names) {
    std::string n = NormalizeName(name);
    normalized.push_back(n);
    if (n != name) {
      denormalized[n] = name;
    }
  }

  // Postconditions:
  for (const auto& pair : denormalized) {
    CHECK_NE(pair.first, pair.second);
    CHECK_EQ(pair.first, NormalizeName(pair.second));
  }

  return std::tie(normalized, denormalized);
}

// Given a list of flag names, adds test failures for any that do not appear in
// alphabetical order. This is more complex than simply sorting the list and
// checking whether the order changed - this function is supposed to emit error
// messages which tell the user specifically which flags need to be moved and to
// where in the file.
void EnsureNamesAreAlphabetical(const NameVector& names,
                                const std::string& filename) {
  auto [normalized, denormalized] = NormalizeNames(names);
  auto was_after = BuildAfterMap(normalized);

  std::sort(normalized.begin(), normalized.end());
  auto goes_after = BuildAfterMap(normalized);

  auto denormalize = [&](const std::string& name) {
    return denormalized.contains(name) ? denormalized[name] : name;
  };

  for (const auto& n : normalized) {
    if (was_after[n] != goes_after[n]) {
      ADD_FAILURE() << "In '" << filename << "': flag '" << denormalize(n)
                    << "' should be right after '" << denormalize(goes_after[n])
                    << "'";
    }
  }
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
  {
    auto json = ReadFileContentsAsJSON(kMetadataFileName);
    std::vector<std::string> names;
    for (const auto& entry : json.GetList()) {
      names.push_back(*entry.GetDict().FindString("name"));
    }
    EnsureNamesAreAlphabetical(names, kMetadataFileName);
  }

  {
    auto json = ReadFileContentsAsJSON(kNeverExpireFileName);
    std::vector<std::string> names;
    for (const auto& entry : json.GetList()) {
      names.push_back(entry.GetString());
    }

    EnsureNamesAreAlphabetical(names, kNeverExpireFileName);
  }
}

// TODO(crbug.com/40785799): Call this from the iOS flags unittests once
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
