// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_mojom_traits.h"

#include "base/test/gtest_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings.mojom-shared.h"
#include "components/content_settings/core/common/content_settings.mojom.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

TEST(ContentSettingsTraitsTest, Roundtrips_PatternParts) {
  ContentSettingsPattern::PatternParts original;
  original.has_domain_wildcard = true;
  original.is_path_wildcard = true;
  original.is_port_wildcard = true;
  original.is_scheme_wildcard = true;
  original.port = "8080";
  original.scheme = "scheme";
  original.path = "path";
  ContentSettingsPattern::PatternParts round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              content_settings::mojom::PatternParts>(original, round_tripped));

  EXPECT_EQ(original, round_tripped);
}

TEST(ContentSettingsTraitsTest, Roundtrips_ContentSettingsPattern) {
  ContentSettingsPattern original =
      ContentSettingsPattern::FromString("https://example.com:*");
  ContentSettingsPattern round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              content_settings::mojom::ContentSettingsPattern>(original,
                                                               round_tripped));

  EXPECT_EQ(original, round_tripped);
}

TEST(ContentSettingsTraitsTest, Roundtrips_ContentSetting) {
  for (ContentSetting original : {
           CONTENT_SETTING_DEFAULT,
           CONTENT_SETTING_ALLOW,
           CONTENT_SETTING_BLOCK,
           CONTENT_SETTING_ASK,
           CONTENT_SETTING_SESSION_ONLY,
           CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
       }) {
    ContentSetting round_tripped;

    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<
            content_settings::mojom::ContentSetting>(original, round_tripped));

    EXPECT_EQ(original, round_tripped);
  }
}

TEST(ContentSettingsTraitsTest, Roundtrips_RuleMetadata) {
  content_settings::RuleMetaData original;
  original.set_last_modified(base::Time::FromSecondsSinceUnixEpoch(123));
  original.set_last_visited(base::Time::FromSecondsSinceUnixEpoch(234));
  original.SetExpirationAndLifetime(base::Time::FromSecondsSinceUnixEpoch(345),
                                    base::Days(2));
  original.set_session_model(
      content_settings::mojom::SessionModel::USER_SESSION);
  original.set_tpcd_metadata_rule_source(
      content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);
  original.set_tpcd_metadata_cohort(
      content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_OFF);
  original.set_decided_by_related_website_sets(true);

  content_settings::RuleMetaData round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              content_settings::mojom::RuleMetaData>(original, round_tripped));

  EXPECT_EQ(original, round_tripped);
}

TEST(ContentSettingsTraitsTest, Roundtrips_ContentSettingPatternSource) {
  ContentSettingPatternSource original;
  original.primary_pattern =
      ContentSettingsPattern::FromString("https://example.com:*");
  original.secondary_pattern =
      ContentSettingsPattern::FromString("https://foo.com:*");
  original.incognito = true;
  original.setting_value = base::Value(123);
  original.metadata.SetExpirationAndLifetime(
      base::Time::FromSecondsSinceUnixEpoch(234), base::Days(2));
  original.source = content_settings::ProviderType::kNone;
  ContentSettingPatternSource round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              content_settings::mojom::ContentSettingPatternSource>(
      original, round_tripped));

  EXPECT_EQ(original, round_tripped);
}

TEST(ContentSettingsTraitsTest, Roundtrips_RendererContentSettingRules) {
  RendererContentSettingRules original;

  ContentSettingPatternSource source;

  source.primary_pattern =
      ContentSettingsPattern::FromString("https://mixed-content.com:*");
  original.mixed_content_rules = {source};

  RendererContentSettingRules round_tripped;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              content_settings::mojom::RendererContentSettingRules>(
      original, round_tripped));

  EXPECT_EQ(original, round_tripped);
}

}  // namespace mojo
