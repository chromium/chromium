// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/content_settings/core/browser/host_indexed_content_settings.h"

#include "components/content_settings/core/common/content_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

ContentSettingPatternSource CreateSetting(
    const std::string& primary_pattern,
    const std::string& secondary_pattern,
    ContentSetting setting,
    base::Time expiration = base::Time()) {
  content_settings::RuleMetaData metadata;
  metadata.SetExpirationAndLifetime(
      expiration, expiration.is_null() ? base::TimeDelta()
                                       : expiration - base::Time::Now());
  return ContentSettingPatternSource(
      ContentSettingsPattern::FromString(primary_pattern),
      ContentSettingsPattern::FromString(secondary_pattern),
      base::Value(setting), std::string(), false /* incognito */, metadata);
}

class HostIndexedContentSettingsTest : public testing::Test {
 public:
  HostIndexedContentSettingsTest() = default;
};

TEST_F(HostIndexedContentSettingsTest, EmptyHostIndexedContentSettings) {
  HostIndexedContentSettings test_settings_map = {};
  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("https://www.example.com/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map),
            absl::nullopt);
}
TEST_F(HostIndexedContentSettingsTest, DomainWildcardMatchFound) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_BLOCK),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("https://www.example.com/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map)
                .value(),
            CONTENT_SETTING_ALLOW);
}

TEST_F(HostIndexedContentSettingsTest, MostSpecificMatchBlocks) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("www.example.com/123", "*", CONTENT_SETTING_BLOCK),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("https://www.example.com/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map)
                .value(),
            CONTENT_SETTING_BLOCK);
}

TEST_F(HostIndexedContentSettingsTest, ExactDomainMatchFound) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_ALLOW),
      CreateSetting("https://www.example.com/*", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "*", CONTENT_SETTING_BLOCK),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("https://www.example.com/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map)
                .value(),
            CONTENT_SETTING_ALLOW);
}

TEST_F(HostIndexedContentSettingsTest, NotFirstDomainMatchFound) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("https://www.example.com/*", "[*.]example.com",
                    CONTENT_SETTING_BLOCK),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("https://www.example.com/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map)
                .value(),
            CONTENT_SETTING_ALLOW);
}

TEST_F(HostIndexedContentSettingsTest, WildcardMatchFound) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("*", "[*.]toplevel.com", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("https://www.example.com:123/", "[*.]toplevel.com",
                    CONTENT_SETTING_BLOCK),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("https://www.example.com/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map)
                .value(),
            CONTENT_SETTING_ALLOW);
}

TEST_F(HostIndexedContentSettingsTest, NoMatchFound) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_BLOCK),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("https://www.example.com:123/", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(
                GURL("https://www.example.com:456/"),
                GURL("http://toplevel.com"), test_settings_map),
            absl::nullopt);
}

TEST_F(HostIndexedContentSettingsTest, CheckIPAddressesMatch) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("192.168.1.2", "[*.]toplevel.com", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[a:b:c:d:e:f:0:1]", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "*", CONTENT_SETTING_DEFAULT),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("http://192.168.1.2/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map)
                .value(),
            CONTENT_SETTING_ALLOW);
}

TEST_F(HostIndexedContentSettingsTest, CheckIPAddressesMatchIsBlock) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("192.168.1.2", "[*.]toplevel.com", CONTENT_SETTING_BLOCK),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[a:b:c:d:e:f:0:1]", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "*", CONTENT_SETTING_DEFAULT),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("http://192.168.1.2/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map)
                .value(),
            CONTENT_SETTING_BLOCK);
}

TEST_F(HostIndexedContentSettingsTest, CheckIPAddressesNoMatch) {
  ContentSettingsForOneType test_settings = {
      CreateSetting("192.168.1.2", "[*.]example.com", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[a:b:c:d:e:f:0:1]", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_BLOCK),
      CreateSetting("[*.]example.com", "*", CONTENT_SETTING_DEFAULT),
  };
  HostIndexedContentSettings test_settings_map =
      ToHostIndexedMap(test_settings);

  EXPECT_EQ(FindInHostIndexedContentSettings(GURL("http://192.168.1.2/"),
                                             GURL("http://toplevel.com"),
                                             test_settings_map),
            absl::nullopt);
}

class FindContentSettingTest : public testing::Test {
 public:
  FindContentSettingTest() = default;
};

TEST_F(FindContentSettingTest, MatchInMultiItemVector) {
  ContentSettingsForOneType matching_vector = {
      CreateSetting("https://www.example.com:*/*", "*", CONTENT_SETTING_BLOCK),
      CreateSetting("*://www.example.com:123/*", "[*.]example.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]example.com", "[*.]example.com",
                    CONTENT_SETTING_ALLOW)};
  EXPECT_EQ(FindContentSetting(GURL("https://www.example.com/"),
                               GURL("http://toplevel.com"), matching_vector)
                .value(),
            CONTENT_SETTING_BLOCK);
}
TEST_F(FindContentSettingTest, MatchInSingleItemVector) {
  ContentSettingsForOneType single_item_vector = {
      CreateSetting("https://www.example.com:*/*", "*", CONTENT_SETTING_ALLOW)};
  EXPECT_EQ(FindContentSetting(GURL("https://www.example.com/"),
                               GURL("http://toplevel.com"), single_item_vector)
                .value(),
            CONTENT_SETTING_ALLOW);
}
TEST_F(FindContentSettingTest, NoMatchInSingleItemVector) {
  ContentSettingsForOneType not_matching_vector = {
      CreateSetting("https://www.example.com:*/*", "[*.]example.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("*://www.example.com:123/*", "[*.]example.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]example.com", "[*.]example.com",
                    CONTENT_SETTING_ALLOW)};
  EXPECT_EQ(
      FindContentSetting(GURL("https://www.example.com/"),
                         GURL("http://toplevel.com"), not_matching_vector),
      absl::nullopt);
}

}  // namespace
}  // namespace content_settings
