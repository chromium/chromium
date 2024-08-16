// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/common/manager_base.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::metadata::common {

class ManagerBaseTest : public testing::Test {
 public:
  ManagerBaseTest() = default;
  ~ManagerBaseTest() override = default;

  ManagerBase* GetManagerBase() {
    if (!manager_base_) {
      manager_base_ = std::make_unique<ManagerBase>();
    }
    return manager_base_.get();
  }

 private:
  std::unique_ptr<ManagerBase> manager_base_;
};

class ManagerBaseFeatureTest
    : public ManagerBaseTest,
      public testing::WithParamInterface</*kTpcdMetadataGrants*/ bool> {
 public:
  ManagerBaseFeatureTest() = default;
  ~ManagerBaseFeatureTest() override = default;

  bool IsTpcdMetadataGrantsEnabled() { return GetParam(); }

 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (IsTpcdMetadataGrantsEnabled()) {
      enabled_features.push_back(net::features::kTpcdMetadataGrants);
    } else {
      disabled_features.push_back(net::features::kTpcdMetadataGrants);
    }

    scoped_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         ManagerBaseFeatureTest,
                         testing::Bool());

TEST_P(ManagerBaseFeatureTest, GetContentSetting) {
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  GURL first_party_url = GURL(secondary_pattern_spec);
  GURL third_party_url = GURL(primary_pattern_spec);
  GURL third_party_url_no_grants = GURL("https://www.bar.com");

  base::Value value(ContentSetting::CONTENT_SETTING_ALLOW);
  content_settings::RuleMetaData rule_metadata;
  rule_metadata.set_tpcd_metadata_rule_source(
      content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);

  ContentSettingsForOneType grants;
  grants.emplace_back(
      ContentSettingsPattern::FromString(primary_pattern_spec),
      ContentSettingsPattern::FromString(secondary_pattern_spec),
      std::move(value), content_settings::ProviderType::kNone,
      /*incognito=*/false, rule_metadata);

  auto index = std::move(
      content_settings::HostIndexedContentSettings::Create(grants).front());

  {
    content_settings::SettingInfo out_info;
    EXPECT_EQ(GetManagerBase()->GetContentSetting(index, third_party_url,
                                                  first_party_url, &out_info),
              IsTpcdMetadataGrantsEnabled() ? CONTENT_SETTING_ALLOW
                                            : CONTENT_SETTING_BLOCK);
    EXPECT_EQ(out_info.primary_pattern.ToString(),
              IsTpcdMetadataGrantsEnabled() ? primary_pattern_spec : "*");
    EXPECT_EQ(out_info.secondary_pattern.ToString(),
              IsTpcdMetadataGrantsEnabled() ? secondary_pattern_spec : "*");
    EXPECT_EQ(out_info.metadata.tpcd_metadata_rule_source(),
              IsTpcdMetadataGrantsEnabled()
                  ? content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST
                  : content_settings::mojom::TpcdMetadataRuleSource::
                        SOURCE_UNSPECIFIED);
  }

    {
      content_settings::SettingInfo out_info;
      EXPECT_EQ(
          GetManagerBase()->GetContentSetting(index, third_party_url_no_grants,
                                              first_party_url, &out_info),
          CONTENT_SETTING_BLOCK);
      EXPECT_EQ(out_info.primary_pattern.ToString(), "*");
      EXPECT_EQ(out_info.secondary_pattern.ToString(), "*");
      EXPECT_EQ(
          out_info.metadata.tpcd_metadata_rule_source(),
          content_settings::mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED);
    }
}

class ManagerBaseCohortTest
    : public ManagerBaseTest,
      public testing::WithParamInterface<
          /*content_settings::mojom::TpcdMetadataCohort:*/ int32_t> {
 public:
  ManagerBaseCohortTest() = default;
  ~ManagerBaseCohortTest() override = default;

  content_settings::mojom::TpcdMetadataCohort GetCohortBeingTested() {
    return static_cast<content_settings::mojom::TpcdMetadataCohort>(GetParam());
  }

  ContentSetting ExpectedContentSetting(
      const content_settings::mojom::TpcdMetadataCohort cohort) {
    switch (cohort) {
      case content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_OFF:
        return CONTENT_SETTING_BLOCK;
      case content_settings::mojom::TpcdMetadataCohort::DEFAULT:
      case content_settings::mojom::TpcdMetadataCohort::GRACE_PERIOD_FORCED_ON:
        return CONTENT_SETTING_ALLOW;
    }

    NOTREACHED() << "Invalid enum value: " << GetCohortBeingTested();
  }

 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(net::features::kTpcdMetadataGrants);

    scoped_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_list_;
  std::unique_ptr<ManagerBase> manager_base_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ManagerBaseCohortTest,
    testing::Range<int32_t>(
        static_cast<int32_t>(
            content_settings::mojom::TpcdMetadataCohort::kMinValue),
        static_cast<int32_t>(
            content_settings::mojom::TpcdMetadataCohort::kMaxValue) +
            1,
        /*step=*/1));

TEST_P(ManagerBaseCohortTest, GetContentSetting) {
  const std::string primary_pattern_spec = "https://www.der.com";
  const std::string secondary_pattern_spec = "https://www.foo.com";

  GURL first_party_url = GURL(secondary_pattern_spec);
  GURL third_party_url = GURL(primary_pattern_spec);
  GURL third_party_url_no_grants = GURL("https://www.bar.com");

  base::Value value(ContentSetting::CONTENT_SETTING_ALLOW);
  content_settings::RuleMetaData rule_metadata;
  rule_metadata.set_tpcd_metadata_rule_source(
      content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);
  rule_metadata.set_tpcd_metadata_cohort(GetCohortBeingTested());

  ContentSettingsForOneType grants;
  grants.emplace_back(
      ContentSettingsPattern::FromString(primary_pattern_spec),
      ContentSettingsPattern::FromString(secondary_pattern_spec),
      std::move(value), content_settings::ProviderType::kNone,
      /*incognito=*/false, rule_metadata);

  auto index = std::move(
      content_settings::HostIndexedContentSettings::Create(grants).front());

  {
    content_settings::SettingInfo out_info;
    EXPECT_EQ(GetManagerBase()->GetContentSetting(index, third_party_url,
                                                  first_party_url, &out_info),
              ExpectedContentSetting(GetCohortBeingTested()));
    EXPECT_EQ(out_info.primary_pattern.ToString(), primary_pattern_spec);
    EXPECT_EQ(out_info.secondary_pattern.ToString(), secondary_pattern_spec);
    EXPECT_EQ(out_info.metadata.tpcd_metadata_rule_source(),
              content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);
    EXPECT_EQ(out_info.metadata.tpcd_metadata_cohort(), GetCohortBeingTested());
  }

  {
    content_settings::SettingInfo out_info;
    EXPECT_EQ(GetManagerBase()->GetContentSetting(
                  index, third_party_url_no_grants, first_party_url, &out_info),
              CONTENT_SETTING_BLOCK);
    EXPECT_EQ(out_info.primary_pattern.ToString(), "*");
    EXPECT_EQ(out_info.secondary_pattern.ToString(), "*");
    EXPECT_EQ(
        out_info.metadata.tpcd_metadata_rule_source(),
        content_settings::mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED);
  }
}
}  // namespace tpcd::metadata::common
