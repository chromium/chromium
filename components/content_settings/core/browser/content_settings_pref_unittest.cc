// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/stl_util.h"
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

constexpr char kLastModifiedKey[] = "last_modified";
constexpr char kSettingKey[] = "setting";
constexpr char kTagKey[] = "tag";

#if BUILDFLAG(ENABLE_PLUGINS)
constexpr char kPluginsContentSettingPrefName[] = "content_settings.plugins";
constexpr char kPerResourceTag[] = "per_resource";

// Tests that a particular pref has the expected values with and without a
// resource id
void LegacyPersistedPluginTests(
    ContentSettingsPref* content_settings_pref,
    const std::string& pattern,
    const GURL& host,
    const std::string& resource,
    ContentSetting no_resource_id_perf_expected_value,
    ContentSetting with_resource_id_perf_expected_value) {
  auto pattern_pair = ParsePatternString(pattern);
  // Retrieving the pref without a resource id for pattern works and
  // its value is the expected one.
  EXPECT_EQ(no_resource_id_perf_expected_value,
            content_settings::ValueToContentSetting(
                content_settings::TestUtils::GetContentSettingValueAndPatterns(
                    content_settings_pref->GetRuleIterator("", false).get(),
                    host, GURL(), &(pattern_pair.first), &(pattern_pair.second))
                    .get()));

  // Retrieving the pref with a resource id will throw.
  EXPECT_DCHECK_DEATH(
      content_settings_pref->GetRuleIterator(resource, false)->HasNext());

  // Allow resource ids for testing in order to test that the perf was correctly
  // loaded from the json. This basically verifies that we did build a correct
  // json and it was parsed and loaded without any issues.
  content_settings_pref->set_allow_resource_identifiers_for_testing();
  EXPECT_EQ(
      with_resource_id_perf_expected_value,
      content_settings::ValueToContentSetting(
          content_settings::TestUtils::GetContentSettingValueAndPatterns(
              content_settings_pref->GetRuleIterator(resource, false).get(),
              host, GURL(), &(pattern_pair.first), &(pattern_pair.second))
              .get()));
  content_settings_pref->reset_allow_resource_identifiers_for_testing();
}

#endif  // BUILDFLAG(ENABLE_PLUGINS)

// Creates a JSON dictionary representing a dummy content setting exception
// value in preferences. The setting will be marked with the |tag| like so:
//
//   value = {
//     "last_modified": "...",
//     "setting": {
//       "tag": "...",
//     }
//   }
base::Value CreateDummyContentSettingValue(base::StringPiece tag) {
  base::Value setting(base::Value::Type::DICTIONARY);
  setting.SetKey(kTagKey, base::Value(tag));

  base::Value pref_value(base::Value::Type::DICTIONARY);
  pref_value.SetKey(kLastModifiedKey, base::Value("13189876543210000"));
  pref_value.SetKey(kSettingKey, std::move(setting));
  return pref_value;
}

// Given the JSON dictionary representing the "setting" stored under a content
// setting exception value, returns the tag.
std::string GetTagFromDummyContentSetting(const base::Value& setting) {
  const auto* tag = setting.FindKey(kTagKey);
  return tag ? tag->GetString() : std::string();
}

// Given the JSON dictionary representing a content setting exception value,
// returns the tag.
std::string GetTagFromDummyContentSettingValue(const base::Value& pref_value) {
  const auto* setting = pref_value.FindKey(kSettingKey);
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

  auto original_pref_value = std::make_unique<base::DictionaryValue>();
  for (const auto* pattern : kTestOriginalPatterns) {
    original_pref_value->SetKey(pattern,
                                CreateDummyContentSettingValue(pattern));
  }

  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDictionaryPref(kTestContentSettingPrefName);
  prefs.SetUserPref(kTestContentSettingPrefName,
                    std::move(original_pref_value));

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  ContentSettingsPref content_settings_pref(
      ContentSettingsType::MEDIA_ENGAGEMENT, &prefs, &registrar,
      kTestContentSettingPrefName, false, base::DoNothing());

  // Verify that the |value_map| contains the expected content setting patterns
  // and setting.

  std::vector<CanonicalPatternToTag> patterns_to_tags_in_memory;
  auto rule_iterator = content_settings_pref.GetRuleIterator(
      std::string() /* resource_identifier */, false /* is_incognito */);
  while (rule_iterator->HasNext()) {
    auto rule = rule_iterator->Next();
    patterns_to_tags_in_memory.emplace_back(
        CreatePatternString(rule.primary_pattern, rule.secondary_pattern),
        GetTagFromDummyContentSetting(rule.value));
  }

  EXPECT_THAT(patterns_to_tags_in_memory,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));

  // Verify that Preferences do, as well.

  std::vector<CanonicalPatternToTag> patterns_to_tags_in_prefs;
  const auto* canonical_pref_value =
      prefs.GetUserPref(kTestContentSettingPrefName);
  ASSERT_TRUE(canonical_pref_value->is_dict());
  for (const auto& key_value : canonical_pref_value->DictItems()) {
    patterns_to_tags_in_prefs.emplace_back(
        key_value.first, GetTagFromDummyContentSettingValue(key_value.second));
  }

  EXPECT_THAT(patterns_to_tags_in_prefs,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Test that a legagcy persisted plugin setting does not cause errors and has
// a sane behaviour.
TEST(ContentSettingsPref, LegacyPersistedPluginSetting) {
  const GURL kHost1("http://example.com/");
  const GURL kHost2("http://other-example.com/");
  constexpr char kPattern1[] = "http://example.com,*";
  constexpr char kPattern2[] = "http://other-example.com,*";
  constexpr char kResource[] = "someplugin";

  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDictionaryPref(kPluginsContentSettingPrefName);

  // Build a json simulating some pre-existing plugin settings situation where
  // a mix of per_resource and regular settings are present:
  // "content_settings.plugins": {
  //   kPattern1: {
  //    "setting": 1, <-- CONTENT_SETTING_ALLOW
  //    "per_resource": {
  //      "someplugin": 2 <-- CONTENT_SETTING_BLOCK
  //    }
  //   }
  //   kPattern2: {
  //    "per_resource": {
  //      "someplugin": 1 <-- CONTENT_SETTING_ALLOW
  //    }
  //   }
  // }

  auto original_pref_value = std::make_unique<base::DictionaryValue>();

  base::Value per_resource_value1(base::Value::Type::DICTIONARY);
  per_resource_value1.SetKey(kResource, base::Value(CONTENT_SETTING_BLOCK));

  base::Value pref_value1(base::Value::Type::DICTIONARY);
  pref_value1.SetKey(kLastModifiedKey, base::Value("13189876543210000"));
  pref_value1.SetKey(kSettingKey, base::Value(CONTENT_SETTING_ALLOW));
  pref_value1.SetKey(kPerResourceTag, std::move(per_resource_value1));

  original_pref_value->SetKey(kPattern1, std::move(pref_value1));

  base::Value per_resource_value2(base::Value::Type::DICTIONARY);
  per_resource_value2.SetKey(kResource, base::Value(CONTENT_SETTING_ALLOW));

  base::Value pref_value2(base::Value::Type::DICTIONARY);
  pref_value2.SetKey(kLastModifiedKey, base::Value("13189876543210000"));
  pref_value2.SetKey(kPerResourceTag, std::move(per_resource_value2));

  original_pref_value->SetKey(kPattern2, std::move(pref_value2));

  prefs.SetUserPref(kPluginsContentSettingPrefName,
                    std::move(original_pref_value));

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  ContentSettingsPref content_settings_pref(
      ContentSettingsType::PLUGINS, &prefs, &registrar,
      kPluginsContentSettingPrefName, false, base::DoNothing());

  // For kPattern1 retrieving the setting without a resource id returns the
  // CONTENT_SETTING_ALLOW value and retrieving it with the resource id (after
  // allowing resource ids for testing) returns CONTENT_SETTING_BLOCK.
  LegacyPersistedPluginTests(&content_settings_pref, kPattern1, kHost1,
                             kResource, CONTENT_SETTING_ALLOW,
                             CONTENT_SETTING_BLOCK);

  // For kPattern2 retrieving the setting without a resource id returns the
  // CONTENT_SETTING_DEFAULT value since it was not set in the first place and
  // retrieving it with the resource id (after allowing resource ids for
  // testing) returns CONTENT_SETTING_ALLOW.
  LegacyPersistedPluginTests(&content_settings_pref, kPattern2, kHost2,
                             kResource, CONTENT_SETTING_DEFAULT,
                             CONTENT_SETTING_ALLOW);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace content_settings
