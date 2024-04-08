// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/common/manager_base.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::metadata::common {

class ManagerBaseTest
    : public testing::Test,
      public testing::WithParamInterface</*kTpcdMetadataGrants*/ bool> {
 public:
  ManagerBaseTest() = default;
  ~ManagerBaseTest() override = default;

  bool IsTpcdMetadataGrantsEnabled() { return GetParam(); }

  ManagerBase* GetManagerBase() {
    if (!manager_base_) {
      manager_base_ = std::make_unique<ManagerBase>();
    }
    return manager_base_.get();
  }

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
  std::unique_ptr<ManagerBase> manager_base_;
};

INSTANTIATE_TEST_SUITE_P(/* no label */, ManagerBaseTest, testing::Bool());

TEST_P(ManagerBaseTest, GetContentSetting) {
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
      std::move(value),
      /*source=*/std::string(), /*incognito=*/false, rule_metadata);

  std::vector<common::Grants> v_grants;
  v_grants.emplace_back(grants);
  v_grants.emplace_back(std::move(
      content_settings::HostIndexedContentSettings::Create(grants).front()));

  for (const auto& itr : v_grants) {
    {
      content_settings::SettingInfo out_info;
      EXPECT_EQ(GetManagerBase()->GetContentSetting(itr, third_party_url,
                                                    first_party_url, &out_info),
                IsTpcdMetadataGrantsEnabled() ? CONTENT_SETTING_ALLOW
                                              : CONTENT_SETTING_BLOCK);
      EXPECT_EQ(out_info.primary_pattern.ToString(),
                IsTpcdMetadataGrantsEnabled() ? primary_pattern_spec : "*");
      EXPECT_EQ(out_info.secondary_pattern.ToString(),
                IsTpcdMetadataGrantsEnabled() ? secondary_pattern_spec : "*");
      EXPECT_EQ(
          out_info.metadata.tpcd_metadata_rule_source(),
          IsTpcdMetadataGrantsEnabled()
              ? content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST
              : content_settings::mojom::TpcdMetadataRuleSource::
                    SOURCE_UNSPECIFIED);
    }

    {
      content_settings::SettingInfo out_info;
      EXPECT_EQ(GetManagerBase()->GetContentSetting(
                    itr, third_party_url_no_grants, first_party_url, &out_info),
                CONTENT_SETTING_BLOCK);
      EXPECT_EQ(out_info.primary_pattern.ToString(), "*");
      EXPECT_EQ(out_info.secondary_pattern.ToString(), "*");
      EXPECT_EQ(
          out_info.metadata.tpcd_metadata_rule_source(),
          content_settings::mojom::TpcdMetadataRuleSource::SOURCE_UNSPECIFIED);
    }
  }
}

}  // namespace tpcd::metadata::common
