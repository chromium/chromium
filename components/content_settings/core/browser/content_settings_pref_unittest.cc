// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
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
constexpr char kTestContentSettingPartitionedPrefName[] =
    "content_settings.test_partitioned";

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

class ContentSettingsPrefTest : public testing::Test {
 public:
  void SetUp() override {
    prefs_.registry()->RegisterDictionaryPref(kTestContentSettingPrefName);
    prefs_.registry()->RegisterDictionaryPref(
        kTestContentSettingPartitionedPrefName);

    registrar_.Init(&prefs_);
  }

  std::unique_ptr<ContentSettingsPref> CreateContentSettingsPref(
      ContentSettingsType type) {
    return std::make_unique<ContentSettingsPref>(
        type, &prefs_, &registrar_, kTestContentSettingPrefName,
        kTestContentSettingPartitionedPrefName, false, false,
        base::DoNothing());
  }

  void SetPrefForPartition(const PartitionKey& partition_key,
                           base::Value::Dict partition_settings_dictionary) {
    if (partition_key.is_default()) {
      prefs_.SetUserPref(kTestContentSettingPrefName,
                         std::move(partition_settings_dictionary));
      return;
    }

    SetPrefForNonDefaultPartition(partition_key.Serialize(),
                                  std::move(partition_settings_dictionary));
  }

  void SetPrefForNonDefaultPartition(
      const std::string& serialized_partition_key,
      base::Value::Dict partition_settings_dictionary) {
    prefs::ScopedDictionaryPrefUpdate update(
        &prefs_, kTestContentSettingPartitionedPrefName);
    update->SetDictionaryWithoutPathExpansion(
        serialized_partition_key, std::move(partition_settings_dictionary));
  }

  const base::Value::Dict* GetPrefForPartition(
      const PartitionKey& partition_key) {
    if (partition_key.is_default()) {
      return &prefs_.GetDict(kTestContentSettingPrefName);
    }
    return prefs_.GetDict(kTestContentSettingPartitionedPrefName)
        .FindDict(partition_key.Serialize());
  }

 protected:
  TestingPrefServiceSimple prefs_;
  PrefChangeRegistrar registrar_;
};

class ContentSettingsPrefParameterizedTest
    : public ContentSettingsPrefTest,
      public testing::WithParamInterface<PartitionKey> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentSettingsPrefParameterizedTest,
    testing::Values(PartitionKey::GetDefaultForTesting(),
                    PartitionKey::CreateForTesting(/*domain=*/"foo",
                                                   /*name=*/"bar",
                                                   /*in_memory=*/false)));

TEST_P(ContentSettingsPrefParameterizedTest, BasicReadWrite) {
  const char* pattern_pair = "http://example.com,*";

  SetPrefForPartition(
      GetParam(), base::Value::Dict().Set(
                      pattern_pair, base::Value::Dict().Set(
                                        kSettingKey, CONTENT_SETTING_BLOCK)));
  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::STORAGE_ACCESS);
  auto check_value = [&](ContentSetting expected_value) {
    auto rule_iterator = content_settings_pref->GetRuleIterator(
        /*off_the_record=*/false, GetParam());
    ASSERT_TRUE(rule_iterator->HasNext());
    auto rule = rule_iterator->Next();
    EXPECT_EQ(pattern_pair, CreatePatternString(rule->primary_pattern,
                                                rule->secondary_pattern));
    EXPECT_EQ(rule->value.GetInt(), expected_value);
    EXPECT_FALSE(rule_iterator->HasNext());
  };

  check_value(CONTENT_SETTING_BLOCK);

  content_settings_pref->SetWebsiteSetting(
      ContentSettingsPattern::FromString("http://example.com"),
      ContentSettingsPattern::FromString("*"),
      base::Value(CONTENT_SETTING_ALLOW), RuleMetaData(), GetParam());
  check_value(CONTENT_SETTING_ALLOW);
  // Check that the pref has been updated.
  auto* pref_value = GetPrefForPartition(GetParam());
  EXPECT_EQ(pref_value->size(), 1ul);
  auto* setting = pref_value->FindDict(pattern_pair);
  ASSERT_NE(setting, nullptr);
  EXPECT_EQ(setting->FindInt(kSettingKey),
            std::make_optional<int>(CONTENT_SETTING_ALLOW));
}

TEST_F(ContentSettingsPrefTest,
       ReadContentSettingsFromPrefShouldRemovePartitionIfNecessary) {
  PartitionKey key1 = PartitionKey::CreateForTesting(
      /*domain=*/"foo", /*name=*/"bar", /*in_memory=*/false);
  PartitionKey key2 = PartitionKey::CreateForTesting(
      /*domain=*/"foo", /*name=*/"bar2", /*in_memory=*/false);
  auto data = base::Value::Dict().Set(
      kTestPatternCanonicalAlpha,
      base::Value::Dict().Set(kSettingKey, CONTENT_SETTING_BLOCK));
  SetPrefForPartition(key1, data.Clone());
  SetPrefForPartition(key2, base::Value::Dict());
  SetPrefForNonDefaultPartition("invalid serialized key", data.Clone());

  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::STORAGE_ACCESS);
  // Only key2 should remain in the pref.
  auto& dict = prefs_.GetDict(kTestContentSettingPartitionedPrefName);
  EXPECT_EQ(dict.size(), 1ul);
  EXPECT_NE(dict.FindDict(key1.Serialize()), nullptr);
}

TEST_P(ContentSettingsPrefParameterizedTest,
       SetWebsiteSettingShouldRemovePartitionIfEmpty) {
  SetPrefForPartition(GetParam(), base::Value::Dict().Set(
                                      "http://example.com,*",
                                      base::Value::Dict().Set(
                                          kSettingKey, CONTENT_SETTING_BLOCK)));
  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::STORAGE_ACCESS);
  content_settings_pref->SetWebsiteSetting(
      ContentSettingsPattern::FromString("http://example.com"),
      ContentSettingsPattern::FromString("*"), base::Value(), RuleMetaData(),
      GetParam());
  EXPECT_EQ(content_settings_pref->GetRuleIterator(/*off_the_record=*/false,
                                                   GetParam()),
            nullptr);
  // For non-default partition, the top-level dict containing data for the
  // partition will be removed.
  if (!GetParam().is_default()) {
    EXPECT_EQ(GetPrefForPartition(GetParam()), nullptr);
  }
}

TEST_P(ContentSettingsPrefParameterizedTest, SetWebsiteSettingWhenEmpty) {
  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::STORAGE_ACCESS);
  content_settings_pref->SetWebsiteSetting(
      ContentSettingsPattern::FromString("http://example.com"),
      ContentSettingsPattern::FromString("*"),
      base::Value(CONTENT_SETTING_ALLOW), RuleMetaData(), GetParam());

  auto rule_iterator = content_settings_pref->GetRuleIterator(
      /*off_the_record=*/false, GetParam());
  ASSERT_TRUE(rule_iterator->HasNext());
  auto rule = rule_iterator->Next();
  EXPECT_EQ(
      "http://example.com,*",
      CreatePatternString(rule->primary_pattern, rule->secondary_pattern));
  EXPECT_EQ(rule->value.GetInt(), CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(rule_iterator->HasNext());

  // Check pref.
  auto* pref_value = GetPrefForPartition(GetParam());
  EXPECT_EQ(pref_value->size(), 1ul);
  auto* setting = pref_value->FindDict("http://example.com,*");
  ASSERT_NE(setting, nullptr);
  EXPECT_EQ(setting->FindInt(kSettingKey),
            std::make_optional<int>(CONTENT_SETTING_ALLOW));
}

TEST_F(ContentSettingsPrefTest, DoNotPersistInMemoryPartition) {
  PartitionKey normal_pk = PartitionKey::CreateForTesting(
      /*domain=*/"foo", /*name=*/"bar1", /*in_memory=*/false);
  PartitionKey in_memory_pk = PartitionKey::CreateForTesting(
      /*domain=*/"foo", /*name=*/"bar2", /*in_memory=*/true);

  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::STORAGE_ACCESS);

  for (const auto& partition_key : {normal_pk, in_memory_pk}) {
    content_settings_pref->SetWebsiteSetting(
        ContentSettingsPattern::FromString("http://example.com"),
        ContentSettingsPattern::FromString("*"),
        base::Value(CONTENT_SETTING_ALLOW), RuleMetaData(), partition_key);
  }

  // The value should still be stored, but for `in_memory_pk`, the value will
  // not be written to pref.
  EXPECT_NE(content_settings_pref->GetRuleIterator(/*off_the_record=*/false,
                                                   normal_pk),
            nullptr);
  EXPECT_NE(content_settings_pref->GetRuleIterator(/*off_the_record=*/false,
                                                   in_memory_pk),
            nullptr);
  EXPECT_NE(GetPrefForPartition(normal_pk), nullptr);
  EXPECT_EQ(GetPrefForPartition(in_memory_pk), nullptr);
}

TEST_F(ContentSettingsPrefTest, ClearAllContentSettingsRules) {
  base::Value::Dict dummy_pref_value = base::Value::Dict().Set(
      kTestPatternCanonicalAlpha,
      base::Value::Dict().Set(kSettingKey, CONTENT_SETTING_BLOCK));
  PartitionKey pk1 =
      PartitionKey::CreateForTesting("foo", "bar1", /*in_memory=*/false);
  PartitionKey pk2 =
      PartitionKey::CreateForTesting("foo", "bar2", /*in_memory=*/false);
  SetPrefForPartition(PartitionKey::GetDefaultForTesting(),
                      dummy_pref_value.Clone());
  SetPrefForPartition(pk1, dummy_pref_value.Clone());
  SetPrefForPartition(pk2, dummy_pref_value.Clone());

  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::STORAGE_ACCESS);
  content_settings_pref->ClearAllContentSettingsRules(
      PartitionKey::GetDefaultForTesting());
  content_settings_pref->ClearAllContentSettingsRules(pk1);

  EXPECT_EQ(content_settings_pref->GetRuleIterator(
                /*off_the_record=*/false, PartitionKey::GetDefaultForTesting()),
            nullptr);
  EXPECT_EQ(
      content_settings_pref->GetRuleIterator(/*off_the_record=*/false, pk1),
      nullptr);
  EXPECT_NE(
      content_settings_pref->GetRuleIterator(/*off_the_record=*/false, pk2),
      nullptr);

  EXPECT_TRUE(
      GetPrefForPartition(PartitionKey::GetDefaultForTesting())->empty());
  EXPECT_EQ(GetPrefForPartition(pk1), nullptr);
  EXPECT_NE(GetPrefForPartition(pk2), nullptr);
}

TEST_F(ContentSettingsPrefTest, GetNumExceptions) {
  SetPrefForPartition(
      PartitionKey::GetDefaultForTesting(),
      base::Value::Dict().Set(
          kTestPatternCanonicalAlpha,
          base::Value::Dict().Set(kSettingKey, CONTENT_SETTING_BLOCK)));
  SetPrefForPartition(
      PartitionKey::CreateForTesting("foo", "bar1", /*in_memory=*/false),
      base::Value::Dict()
          .Set(kTestPatternCanonicalAlpha,
               base::Value::Dict().Set(kSettingKey, CONTENT_SETTING_BLOCK))
          .Set(kTestPatternCanonicalBeta,
               base::Value::Dict().Set(kSettingKey, CONTENT_SETTING_BLOCK)));

  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::STORAGE_ACCESS);
  EXPECT_EQ(content_settings_pref->GetNumExceptions(), 3ul)
      << "It should take all partitions into account";
}

TEST_P(ContentSettingsPrefParameterizedTest,
       CanonicalizationWhileReadingFromPrefs) {
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

  SetPrefForPartition(GetParam(), std::move(original_pref_value));
  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::MEDIA_ENGAGEMENT);

  // Verify that the |value_map| contains the expected content setting patterns
  // and setting.

  std::vector<CanonicalPatternToTag> patterns_to_tags_in_memory;
  auto rule_iterator = content_settings_pref->GetRuleIterator(
      false /* is_incognito */, GetParam());
  while (rule_iterator->HasNext()) {
    auto rule = rule_iterator->Next();
    patterns_to_tags_in_memory.emplace_back(
        CreatePatternString(rule->primary_pattern, rule->secondary_pattern),
        GetTagFromDummyContentSetting(rule->value.GetDict()));
  }

  EXPECT_THAT(patterns_to_tags_in_memory,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));

  // Verify that Preferences do, as well.

  std::vector<CanonicalPatternToTag> patterns_to_tags_in_prefs;
  for (auto key_value : *GetPrefForPartition(GetParam())) {
    patterns_to_tags_in_prefs.emplace_back(
        key_value.first,
        GetTagFromDummyContentSettingValue(key_value.second.GetDict()));
  }

  EXPECT_THAT(patterns_to_tags_in_prefs,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));
}

// If we are reading from prefs and we have any persistend settings that have
// expired we should remove these to prevent unbounded growth and bloat.
TEST_P(ContentSettingsPrefParameterizedTest, ExpirationWhileReadingFromPrefs) {
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

  SetPrefForPartition(GetParam(), std::move(original_pref_value));
  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::MEDIA_ENGAGEMENT);

  // Verify that the |value_map| contains the expected content setting patterns
  // and setting.
  std::vector<CanonicalPatternToTag> patterns_to_tags_in_memory;
  auto rule_iterator = content_settings_pref->GetRuleIterator(
      false /* is_incognito */, GetParam());
  while (rule_iterator->HasNext()) {
    auto rule = rule_iterator->Next();
    patterns_to_tags_in_memory.emplace_back(
        CreatePatternString(rule->primary_pattern, rule->secondary_pattern),
        GetTagFromDummyContentSetting(rule->value.GetDict()));
  }

  EXPECT_THAT(patterns_to_tags_in_memory,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));

  // Verify that Preferences do, as well.
  std::vector<CanonicalPatternToTag> patterns_to_tags_in_prefs;
  for (auto key_value : *GetPrefForPartition(GetParam())) {
    patterns_to_tags_in_prefs.emplace_back(
        key_value.first,
        GetTagFromDummyContentSettingValue(key_value.second.GetDict()));
  }

  EXPECT_THAT(patterns_to_tags_in_prefs,
              testing::UnorderedElementsAreArray(kExpectedPatternsToTags));
}

// Ensure that any previously set last_modified values using
// base::Time::ToInternalValue can be read correctly.
TEST_P(ContentSettingsPrefParameterizedTest, LegacyLastModifiedLoad) {
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

  SetPrefForPartition(GetParam(), std::move(original_pref_value));

  auto content_settings_pref =
      CreateContentSettingsPref(ContentSettingsType::STORAGE_ACCESS);

  // Ensure that after reading from our JSON/old value the last_modified time is
  // still parsed correctly.
  EXPECT_EQ(content_settings_pref->GetNumExceptions(), 1u);
  auto it = content_settings_pref->GetRuleIterator(false, GetParam());
  base::Time retrieved_last_modified = it->Next()->metadata.last_modified();
  EXPECT_EQ(last_modified, retrieved_last_modified);
}

}  // namespace content_settings
