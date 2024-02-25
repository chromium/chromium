// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_partitioned_origin_value_map.h"

#include "base/synchronization/lock.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

const std::string kUrl = "http://example.com";
const std::string kUrl2 = "http://google.com";
const PartitionKey kKey =
    PartitionKey::CreateForTesting("hello", "world", false);
}  // namespace

TEST(PartitionedOriginValueMap, GetValueAndSetValue) {
  PartitionedOriginValueMap map;

  base::AutoLock lock(map.GetLock());
  EXPECT_EQ(map.GetValue(GURL(kUrl), GURL(kUrl), ContentSettingsType::COOKIES,
                         PartitionKey::GetDefaultForTesting()),
            nullptr);
  EXPECT_TRUE(map.SetValue(ContentSettingsPattern::FromString(kUrl),
                           ContentSettingsPattern::FromString("*"),
                           ContentSettingsType::COOKIES,
                           base::Value(CONTENT_SETTING_BLOCK), RuleMetaData(),
                           PartitionKey::GetDefaultForTesting()));
  EXPECT_EQ(*map.GetValue(GURL(kUrl), GURL(kUrl), ContentSettingsType::COOKIES,
                          PartitionKey::GetDefaultForTesting()),
            base::Value(CONTENT_SETTING_BLOCK));

  EXPECT_EQ(
      map.GetValue(GURL(kUrl), GURL(kUrl), ContentSettingsType::COOKIES, kKey),
      nullptr);
  EXPECT_TRUE(map.SetValue(
      ContentSettingsPattern::FromString(kUrl),
      ContentSettingsPattern::FromString("*"), ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_ALLOW), RuleMetaData(), kKey));
  EXPECT_EQ(
      *map.GetValue(GURL(kUrl), GURL(kUrl), ContentSettingsType::COOKIES, kKey),
      base::Value(CONTENT_SETTING_ALLOW));

  EXPECT_FALSE(map.SetValue(
      ContentSettingsPattern::FromString(kUrl),
      ContentSettingsPattern::FromString("*"), ContentSettingsType::COOKIES,
      base::Value(CONTENT_SETTING_ALLOW), RuleMetaData(), kKey))
      << "expecting false for setting the value to the same value";
}

TEST(PartitionedOriginValueMap, GetRuleIterator) {
  PartitionedOriginValueMap map;

  {
    base::AutoLock lock(map.GetLock());
    for (auto& url : {kUrl, kUrl2}) {
      map.SetValue(ContentSettingsPattern::FromString(url),
                   ContentSettingsPattern::FromString("*"),
                   ContentSettingsType::COOKIES,
                   base::Value(CONTENT_SETTING_BLOCK), RuleMetaData(), kKey);
    }
  }

  map.GetLock().AssertNotHeld();
  EXPECT_EQ(map.GetRuleIterator(ContentSettingsType::COOKIES,
                                PartitionKey::GetDefaultForTesting()),
            nullptr);
  map.GetLock().AssertNotHeld();

  auto it = map.GetRuleIterator(ContentSettingsType::COOKIES, kKey);
  map.GetLock().AssertAcquired();
  ASSERT_TRUE(it->HasNext());
  EXPECT_NE(it->Next(), nullptr);
  ASSERT_TRUE(it->HasNext());
  map.GetLock().AssertAcquired();
  EXPECT_NE(it->Next(), nullptr);
  ASSERT_FALSE(it->HasNext());
  it.reset();
  map.GetLock().AssertNotHeld();
}

TEST(PartitionedOriginValueMap, GetRule) {
  PartitionedOriginValueMap map;
  base::AutoLock lock(map.GetLock());

  map.SetValue(ContentSettingsPattern::FromString(kUrl),
               ContentSettingsPattern::FromString("*"),
               ContentSettingsType::COOKIES, base::Value(CONTENT_SETTING_BLOCK),
               RuleMetaData(), kKey);
  EXPECT_NE(
      map.GetRule(GURL(kUrl), GURL("*"), ContentSettingsType::COOKIES, kKey),
      nullptr);
  EXPECT_EQ(
      map.GetRule(GURL(kUrl2), GURL("*"), ContentSettingsType::COOKIES, kKey),
      nullptr);
  EXPECT_EQ(
      map.GetRule(GURL(kUrl), GURL("*"), ContentSettingsType::JAVASCRIPT, kKey),
      nullptr);
  EXPECT_EQ(map.GetRule(GURL(kUrl), GURL("*"), ContentSettingsType::COOKIES,
                        PartitionKey::GetDefaultForTesting()),
            nullptr);
}

TEST(PartitionedOriginValueMap, DeleteValue) {
  PartitionedOriginValueMap map;

  base::AutoLock lock(map.GetLock());
  for (auto& key : {PartitionKey::GetDefaultForTesting(), kKey}) {
    map.SetValue(ContentSettingsPattern::FromString(kUrl),
                 ContentSettingsPattern::FromString("*"),
                 ContentSettingsType::COOKIES,
                 base::Value(CONTENT_SETTING_BLOCK), RuleMetaData(), key);
  }
  for (auto& key : {PartitionKey::GetDefaultForTesting(), kKey}) {
    EXPECT_NE(
        map.GetValue(GURL(kUrl), GURL(kUrl), ContentSettingsType::COOKIES, key),
        nullptr);
  }
  EXPECT_TRUE(map.DeleteValue(ContentSettingsPattern::FromString(kUrl),
                              ContentSettingsPattern::FromString("*"),
                              ContentSettingsType::COOKIES,
                              PartitionKey::GetDefaultForTesting()));
  EXPECT_EQ(map.GetValue(GURL(kUrl), GURL(kUrl), ContentSettingsType::COOKIES,
                         PartitionKey::GetDefaultForTesting()),
            nullptr);
  EXPECT_NE(
      map.GetValue(GURL(kUrl), GURL(kUrl), ContentSettingsType::COOKIES, kKey),
      nullptr);

  EXPECT_TRUE(map.DeleteValue(ContentSettingsPattern::FromString(kUrl),
                              ContentSettingsPattern::FromString("*"),
                              ContentSettingsType::COOKIES, kKey));
  EXPECT_EQ(
      map.GetValue(GURL(kUrl), GURL(kUrl), ContentSettingsType::COOKIES, kKey),
      nullptr);

  EXPECT_FALSE(map.DeleteValue(ContentSettingsPattern::FromString(kUrl),
                               ContentSettingsPattern::FromString("*"),
                               ContentSettingsType::COOKIES, kKey))
      << "expecting false for deleting a non-exist value";
}

TEST(PartitionedOriginValueMap, DeleteValues) {
  PartitionedOriginValueMap map;

  {
    base::AutoLock lock(map.GetLock());
    for (auto& key : {PartitionKey::GetDefaultForTesting(), kKey}) {
      for (auto& url : {kUrl, kUrl2}) {
        map.SetValue(ContentSettingsPattern::FromString(url),
                     ContentSettingsPattern::FromString("*"),
                     ContentSettingsType::COOKIES,
                     base::Value(CONTENT_SETTING_BLOCK), RuleMetaData(), key);
      }
    }
  }
  for (auto& key : {PartitionKey::GetDefaultForTesting(), kKey}) {
    EXPECT_NE(map.GetRuleIterator(ContentSettingsType::COOKIES, key), nullptr);
  }

  {
    base::AutoLock lock(map.GetLock());
    map.DeleteValues(ContentSettingsType::COOKIES,
                     PartitionKey::GetDefaultForTesting());
  }
  EXPECT_EQ(map.GetRuleIterator(ContentSettingsType::COOKIES,
                                PartitionKey::GetDefaultForTesting()),
            nullptr);
  EXPECT_NE(map.GetRuleIterator(ContentSettingsType::COOKIES, kKey), nullptr);

  {
    base::AutoLock lock(map.GetLock());
    map.DeleteValues(ContentSettingsType::COOKIES, kKey);
  }
  EXPECT_EQ(map.GetRuleIterator(ContentSettingsType::COOKIES, kKey), nullptr);
}

}  // namespace content_settings
