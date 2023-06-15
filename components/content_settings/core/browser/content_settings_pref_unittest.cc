// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {
namespace {

// Test content setting pattern pairs in string format. The percent-encoded
// sequence "%61" should get canonicalized to the letter 'a'.
constexpr char kTestPatternCanonicalAlpha[] = "https://alpha.com,*";
constexpr char kTestPatternNonCanonicalAlpha1[] = "https://%61lpha.com,*";
constexpr char kTestPatternNonCanonicalAlpha2[] = "https://alph%61.com,*";
constexpr char kTestPatternCanonicalBeta[] = "https://beta.com,*";
constexpr char kTestPatternNonCanonicalBeta[] = "https://bet%61.com,*";

constexpr char kTestContentSettingPrefName[] = "content_settings.test";

constexpr char kExpirationKey[] = "expiration";
constexpr char kLastModifiedKey[] = "last_modified";
constexpr char kSettingKey[] = "setting";
constexpr char kTagKey[] = "tag";

// Creates a JSON dictionary representing a dummy content setting exception
// value in preferences. The setting will be marked with the |tag| like so:
//
//   value = {
//     "last_modified": "...",
//     "setting": {
//       "tag": "...",
//     }
//   }
base::Value::Dict CreateDummyContentSettingValue(base::StringPiece tag,
                                                 bool expired) {
  return base::Value::Dict()
      .Set(kSettingKey, base::Value::Dict().Set(kTagKey, tag))
      .Set(kLastModifiedKey, "13189876543210000")
      .Set(kExpirationKey, expired ? "13189876543210001" : "0");
}

// Given the JSON dictionary representing the "setting" stored under a content
// setting exception value, returns the tag.
std::string GetTagFromDummyContentSetting(const base::Value::Dict& setting) {
  const std::string* tag = setting.FindString(kTagKey);
  return tag ? *tag : std::string();
}

// Given the JSON dictionary representing a content setting exception value,
// returns the tag.
std::string GetTagFromDummyContentSettingValue(
    const base::Value::Dict& pref_value) {
  const base::Value::Dict* setting = pref_value.FindDict(kSettingKey);
  return setting ? GetTagFromDummyContentSetting(*setting) : std::string();
}

}  // namespace

TEST(ContentSettingsPref, CanonicalizationWhileReadingFromPrefs) {
  // Canonical/non-canonical patterns originally in preferences.
  constexpr const char* kTestOriginalPatterns[] = {
      kTestPatternNonCanonicalAlpha1,
      kTestPatternNonCanonicalAlpha2,
      kTestPatternCanonicalBeta,
      kTestPatternNonCanonicalBeta,
  };

  // Upon construction, ContentSettingPref reads all content setting exception
  // data stored in Preferences for a given content setting. This process also
  // has the side effect that it migrates all data keyed under non-canonical
  // content setting pattern pairs to be keyed under the corresponding canoncial
  // pattern pair, both in Preferences, as well as in ContentSettingPref's
  // in-memory |value_map| representation. There are two edge cases here:
  //
  //   1) If multiple non-canonical patterns map to the same canonical pattern,
  //      the data for the last read pattern is retained, the rest thrown away.
  //   2) We ignore and delete non-canonical pattern pairs if a canonical one
  //      already exists.
  //
  // With regard to the test data, NonCanonicalAlpha1 and NonCanonicalAlpha2
  // would both map to CanonicalAlpha, so the value for the latter should be
  // retained.
  //
  // NonCanonicalBeta would be canonicalized to CanonicalBeta, but because there
  // is already a value under that key, the latter should be retained and the
  // non-canonical value thrown away.
  using CanonicalPatternToTag = std::pair<std::string, std::string>;
  const std::vector<CanonicalPatternToTag> kExpectedPatternsToTags = {
      {kTestPatternCanonicalAlpha, kTestPatternNonCanonicalAlpha2},
      {kTestPatternCanonicalBeta, kTestPatternCanonicalBeta},
  };

  base::Value::Dict original_pref_value;
  for (const auto* pattern : kTestOriginalPatterns) {
    original_pref_value.Set(
        pattern, CreateDummyContentSettingValue(pattern, /*expired=*/false));
  }

  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDictionaryPref(kTestContentSettingPrefName);
  prefs.SetUserPref(kTestContentSettingPrefName,
                    std::move(original_pref_value));

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  ContentSettingsPref content_settings_pref(
      ContentSettingsType::MEDIA_ENGAGEMENT, &prefs, &registrar,
      kTestContentSettingPrefName, false, false, base::DoNothing());

  // Verify that the |value_map| contains the expected content setting patterns
  // and setting.

  std::vector<CanonicalPatternToTag> patterns_to_tags_in_memory;
  auto rule_iterator =
      content_settings_pref.GetRuleIterator(false /* is_incognito */);
  while (rule_iterator->HasNext()) {
    auto rule = rule_iterator->Next();
    patterns_to_tags_in_memory.emplace_back(
        CreatePatternString(rule->primary_pattern, rule->secondary_pattern),
        GetTagFromDummyContentSetting(rule->value().GetDict()));
  }

  EXPECT_THAT(patterns_to_tags_in_memory,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));

  // Verify that Preferences do, as well.

  std::vector<CanonicalPatternToTag> patterns_to_tags_in_prefs;
  const auto* canonical_pref_value =
      prefs.GetUserPref(kTestContentSettingPrefName);
  ASSERT_TRUE(canonical_pref_value->is_dict());
  for (auto key_value : canonical_pref_value->GetDict()) {
    patterns_to_tags_in_prefs.emplace_back(
        key_value.first,
        GetTagFromDummyContentSettingValue(key_value.second.GetDict()));
  }

  EXPECT_THAT(patterns_to_tags_in_prefs,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));
}

// If we are reading from prefs and we have any persistend settings that have
// expired we should remove these to prevent unbounded growth and bloat.
TEST(ContentSettingsPref, ExpirationWhileReadingFromPrefs) {
  // Upon construction, ContentSettingPref reads all content setting exception
  // data stored in Preferences for a given content setting. This process also
  // has the side effect that it clears out expired settings.

  // We should only have our un-expired setting left over.
  using CanonicalPatternToTag = std::pair<std::string, std::string>;
  const std::vector<CanonicalPatternToTag> kExpectedPatternsToTags = {
      {kTestPatternCanonicalBeta, kTestPatternCanonicalBeta},
  };

  // Create two pre-existing entries, one that is expired and one that never
  // expires.
  base::Value::Dict original_pref_value;
  original_pref_value.Set(
      kTestPatternCanonicalAlpha,
      CreateDummyContentSettingValue(kTestPatternCanonicalAlpha,
                                     /*expired=*/true));
  original_pref_value.Set(
      kTestPatternCanonicalBeta,
      CreateDummyContentSettingValue(kTestPatternCanonicalBeta,
                                     /*expired=*/false));

  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDictionaryPref(kTestContentSettingPrefName);
  prefs.SetUserPref(kTestContentSettingPrefName,
                    std::move(original_pref_value));

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  ContentSettingsPref content_settings_pref(
      ContentSettingsType::MEDIA_ENGAGEMENT, &prefs, &registrar,
      kTestContentSettingPrefName, false, false, base::DoNothing());

  // Verify that the |value_map| contains the expected content setting patterns
  // and setting.
  std::vector<CanonicalPatternToTag> patterns_to_tags_in_memory;
  auto rule_iterator =
      content_settings_pref.GetRuleIterator(false /* is_incognito */);
  while (rule_iterator->HasNext()) {
    auto rule = rule_iterator->Next();
    patterns_to_tags_in_memory.emplace_back(
        CreatePatternString(rule->primary_pattern, rule->secondary_pattern),
        GetTagFromDummyContentSetting(rule->value().GetDict()));
  }

  EXPECT_THAT(patterns_to_tags_in_memory,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));

  // Verify that Preferences do, as well.
  std::vector<CanonicalPatternToTag> patterns_to_tags_in_prefs;
  const auto* canonical_pref_value =
      prefs.GetUserPref(kTestContentSettingPrefName);
  ASSERT_TRUE(canonical_pref_value->is_dict());
  for (auto key_value : canonical_pref_value->GetDict()) {
    patterns_to_tags_in_prefs.emplace_back(
        key_value.first,
        GetTagFromDummyContentSettingValue(key_value.second.GetDict()));
  }

  EXPECT_THAT(patterns_to_tags_in_prefs,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));
}

// Ensure that any previously set last_modified values using
// base::Time::ToInternalValue can be read correctly.
TEST(ContentSettingsPref, LegacyLastModifiedLoad) {
  constexpr char kPatternPair[] = "http://example.com,*";

  base::Value::Dict original_pref_value;
  const base::Time last_modified =
      base::Time::FromInternalValue(13189876543210000);

  // Create a single entry using our old internal value for last_modified.
  base::Value::Dict pref_value;
  pref_value.Set(kLastModifiedKey,
                 base::NumberToString(last_modified.ToInternalValue()));
  pref_value.Set(kSettingKey, CONTENT_SETTING_BLOCK);
  pref_value.Set(kExpirationKey, "0");

  original_pref_value.Set(kPatternPair, std::move(pref_value));

  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDictionaryPref(kTestContentSettingPrefName);
  prefs.SetUserPref(kTestContentSettingPrefName,
                    std::move(original_pref_value));

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  ContentSettingsPref content_settings_pref(
      ContentSettingsType::STORAGE_ACCESS, &prefs, &registrar,
      kTestContentSettingPrefName, false, false, base::DoNothing());

  // Ensure that after reading from our JSON/old value the last_modified time is
  // still parsed correctly.
  EXPECT_EQ(content_settings_pref.GetNumExceptions(), 1u);
  auto it = content_settings_pref.GetRuleIterator(false);
  base::Time retrieved_last_modified = it->Next()->metadata.last_modified();
  EXPECT_EQ(last_modified, retrieved_last_modified);
}

}  // namespace content_settings
