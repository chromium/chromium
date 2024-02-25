// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/host_indexed_content_settings.h"

#include <functional>
#include <optional>
#include <string>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_rules.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

ContentSettingPatternSource CreateSetting(const std::string& primary_pattern,
                                          const std::string& secondary_pattern,
                                          ContentSetting setting,
                                          base::Time expiration = base::Time(),
                                          std::string source = std::string()) {
  content_settings::RuleMetaData metadata;
  metadata.SetExpirationAndLifetime(
      expiration, expiration.is_null() ? base::TimeDelta()
                                       : expiration - base::Time::Now());
  return ContentSettingPatternSource(
      ContentSettingsPattern::FromString(primary_pattern),
      ContentSettingsPattern::FromString(secondary_pattern),
      base::Value(setting), source, false /* incognito */, metadata);
}

ContentSettingsForOneType ToVector(const HostIndexedContentSettings& index) {
  ContentSettingsForOneType v;
  for (auto& entry : index) {
    ContentSettingPatternSource source;
    source.primary_pattern = entry.first.primary_pattern;
    source.secondary_pattern = entry.first.secondary_pattern;
    source.setting_value = entry.second.value.Clone();
    source.metadata = entry.second.metadata;
    v.push_back(std::move(source));
  }
  return v;
}

HostIndexedContentSettings FromVector(
    const ContentSettingsForOneType& settings) {
  auto indices = HostIndexedContentSettings::Create(settings);
  EXPECT_EQ(1u, indices.size());
  return std::move(indices.front());
}

class HostIndexedContentSettingsTest : public testing::Test {
 public:
  HostIndexedContentSettingsTest() = default;
};

TEST_F(HostIndexedContentSettingsTest, EmptyHostIndexedContentSettings) {
  GURL test_primary_url("https://www.example.com/");
  GURL test_secondary_url("http://toplevel.com");
  HostIndexedContentSettings index = HostIndexedContentSettings();

  EXPECT_EQ(index.Find(test_primary_url, test_secondary_url), nullptr);
  EXPECT_EQ(index.begin(), index.end());
  EXPECT_TRUE(index.empty());
  EXPECT_EQ(index.size(), 0u);
  EXPECT_THAT(ToVector(index), ::testing::IsEmpty());
}

TEST_F(HostIndexedContentSettingsTest, Sorting) {
  GURL test_primary_url("https://www.example.com/");
  ContentSettingsForOneType test_settings = {
      CreateSetting("a.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]a.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("a.b.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]a.b.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]b.b.b.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("b.b.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]b.b.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("b.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]b.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("c.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]c.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("*:8080", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("*", "*", CONTENT_SETTING_ALLOW),
  };
  HostIndexedContentSettings index = FromVector(test_settings);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, DomainWildcardMatchFound) {
  GURL test_primary_url("https://www.example.com/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_BLOCK),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(ValueToContentSetting(
                index.Find(test_primary_url, test_secondary_url)->second.value),
            CONTENT_SETTING_ALLOW);
  EXPECT_NE(index.begin(), index.end());
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, MostSpecificMatchBlocks) {
  GURL test_primary_url("https://www.example.com/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("www.example.com/123", "*", CONTENT_SETTING_BLOCK),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_ALLOW),
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_ALLOW),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(ValueToContentSetting(
                index.Find(test_primary_url, test_secondary_url)->second.value),
            CONTENT_SETTING_BLOCK);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, SetDelete) {
  for (auto& [primary, secondary] :
       std::vector<std::pair<std::string, std::string>>(
           {{"*", "toplevel.com"}, {"example.com", "toplevel.com"}})) {
    HostIndexedContentSettings index;
    // Insert a setting.
    EXPECT_TRUE(index.SetValue(ContentSettingsPattern::FromString(primary),
                               ContentSettingsPattern::FromString(secondary),
                               base::Value(CONTENT_SETTING_ALLOW),
                               /*metadata=*/{}));
    // Check setting.
    EXPECT_EQ(ValueToContentSetting(index
                                        .Find(GURL("https://example.com"),
                                              GURL("https://toplevel.com"))
                                        ->second.value),
              CONTENT_SETTING_ALLOW);
    EXPECT_FALSE(index.empty());

    // Check that inserting the same setting returns false.
    EXPECT_FALSE(index.SetValue(ContentSettingsPattern::FromString(primary),
                                ContentSettingsPattern::FromString(secondary),
                                base::Value(CONTENT_SETTING_ALLOW),
                                /*metadata=*/{}));

    // Check that inserting the a different value return true.
    EXPECT_TRUE(index.SetValue(ContentSettingsPattern::FromString(primary),
                               ContentSettingsPattern::FromString(secondary),
                               base::Value(CONTENT_SETTING_BLOCK),
                               /*metadata=*/{}));
    // Delete setting.
    EXPECT_TRUE(
        index.DeleteValue(ContentSettingsPattern::FromString(primary),
                          ContentSettingsPattern::FromString(secondary)));
    // Check setting is gone.
    EXPECT_EQ(
        index.Find(GURL("https://example.com"), GURL("https://toplevel.com")),
        nullptr);
    EXPECT_TRUE(index.empty());

    // Check that deleting the setting again returns false.
    EXPECT_FALSE(
        index.DeleteValue(ContentSettingsPattern::FromString(primary),
                          ContentSettingsPattern::FromString(secondary)));
  }
}

TEST_F(HostIndexedContentSettingsTest, ExactDomainMatchFound) {
  GURL test_primary_url("https://www.example.com/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("https://www.example.com/*", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]example.com", "*", CONTENT_SETTING_BLOCK),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_ALLOW),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(ValueToContentSetting(
                index.Find(test_primary_url, test_secondary_url)->second.value),
            CONTENT_SETTING_ALLOW);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, NotFirstDomainMatchFound) {
  GURL test_primary_url("https://www.example.com/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("https://www.example.com/*", "[*.]example.com",
                    CONTENT_SETTING_BLOCK),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_DEFAULT),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(ValueToContentSetting(
                index.Find(test_primary_url, test_secondary_url)->second.value),
            CONTENT_SETTING_ALLOW);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, WildcardMatchFound) {
  GURL test_primary_url("https://www.example.com/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("https://www.example.com:123/", "[*.]toplevel.com",
                    CONTENT_SETTING_BLOCK),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("*", "[*.]toplevel.com", CONTENT_SETTING_ALLOW),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(ValueToContentSetting(
                index.Find(test_primary_url, test_secondary_url)->second.value),
            CONTENT_SETTING_ALLOW);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, NoMatchFound) {
  GURL test_primary_url("https://www.example.com:456/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("https://www.example.com:123/", "[*.]toplevel.com",
                    CONTENT_SETTING_ALLOW),
      CreateSetting("[*.]toplevel.com", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("*", "[*.]example.com", CONTENT_SETTING_BLOCK),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(index.Find(test_primary_url, test_secondary_url), nullptr);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, CheckIPAddressesMatch) {
  GURL test_primary_url("http://192.168.1.2/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("192.168.1.2", "[*.]toplevel.com", CONTENT_SETTING_ALLOW),
      CreateSetting("[a:b:c:d:e:f:0:1]", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(ValueToContentSetting(
                index.Find(test_primary_url, test_secondary_url)->second.value),
            CONTENT_SETTING_ALLOW);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, CheckIPAddressesMatchIsBlock) {
  GURL test_primary_url("http://192.168.1.2/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("192.168.1.2", "[*.]toplevel.com", CONTENT_SETTING_BLOCK),
      CreateSetting("[a:b:c:d:e:f:0:1]", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_DEFAULT),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(ValueToContentSetting(
                index.Find(test_primary_url, test_secondary_url)->second.value),
            CONTENT_SETTING_BLOCK);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
}

TEST_F(HostIndexedContentSettingsTest, CheckIPAddressesNoMatch) {
  GURL test_primary_url("http://192.168.1.2/");
  GURL test_secondary_url("http://toplevel.com");
  ContentSettingsForOneType test_settings = {
      CreateSetting("192.168.1.2", "[*.]example.com", CONTENT_SETTING_ALLOW),
      CreateSetting("[a:b:c:d:e:f:0:1]", "[*.]example.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "[*.]toplevel.com",
                    CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]example.com", "*", CONTENT_SETTING_DEFAULT),
      CreateSetting("[*.]toplevel.com", "*", CONTENT_SETTING_BLOCK),
  };
  HostIndexedContentSettings index = FromVector(test_settings);

  EXPECT_EQ(index.Find(test_primary_url, test_secondary_url), nullptr);
  EXPECT_THAT(ToVector(index), testing::ContainerEq(test_settings));
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
                ->GetContentSetting(),
            CONTENT_SETTING_BLOCK);
}

TEST_F(FindContentSettingTest, MatchInSingleItemVector) {
  ContentSettingsForOneType single_item_vector = {
      CreateSetting("https://www.example.com:*/*", "*", CONTENT_SETTING_ALLOW)};
  EXPECT_EQ(FindContentSetting(GURL("https://www.example.com/"),
                               GURL("http://toplevel.com"), single_item_vector)
                ->GetContentSetting(),
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
      nullptr);
}

TEST_F(FindContentSettingTest, VectorOfIndices) {
  ContentSettingsForOneType settings = {
      CreateSetting("https://example.com:*/*", "*", CONTENT_SETTING_BLOCK,
                    base::Time(), "policy"),
      CreateSetting("*", "*", CONTENT_SETTING_ALLOW, base::Time(), "policy"),
      CreateSetting("[*.]example.com", "[*.]example.com", CONTENT_SETTING_BLOCK,
                    base::Time(), "pref"),
      CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY, base::Time(),
                    "default")};
  auto indices = HostIndexedContentSettings::Create(settings);

  EXPECT_EQ(3u, indices.size());

  ContentSettingsForOneType expected_0 = {
      CreateSetting("https://example.com:*/*", "*", CONTENT_SETTING_BLOCK),
      CreateSetting("*", "*", CONTENT_SETTING_ALLOW)};
  EXPECT_EQ(indices[0].source(), "policy");
  EXPECT_EQ(ToVector(indices[0]), expected_0);

  ContentSettingsForOneType expected_1 = {CreateSetting(
      "[*.]example.com", "[*.]example.com", CONTENT_SETTING_BLOCK)};
  EXPECT_EQ(indices[1].source(), "pref");
  EXPECT_EQ(ToVector(indices[1]), expected_1);

  ContentSettingsForOneType expected_2 = {
      CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)};
  EXPECT_EQ(indices[2].source(), "default");
  EXPECT_EQ(ToVector(indices[2]), expected_2);
}

}  // namespace
}  // namespace content_settings
