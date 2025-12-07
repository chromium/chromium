// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/analysis_service_settings_base.h"

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/analysis_test_utils.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

using test::GetExpectedLearnMoreUrlSpecs;
using test::kEnablePatternIsNotADictSettings;
using test::kNoDlpDotCom;
using test::kNoDlpOrMalwareDotCa;
using test::kNoEnabledPatternsSettings;
using test::kNoMalwareDotCom;
using test::kNoProviderSettings;
using test::kNormalSettings;
using test::kNormalSettingsDlpRequiresBypassJustification;
using test::kNormalSettingsWithCustomMessage;
using test::kOnlyDlpEnabledPatternsAndIrrelevantSettings;
using test::kOnlyDlpEnabledPatternsSettings;
using test::kScan1DotCom;
using test::kScan2DotCom;
using test::kUrlAndSourceDestinationListSettings;
using test::NormalDlpAndMalwareSettings;
using test::NormalDlpSettings;
using test::NormalMalwareSettings;
using test::NormalSettingsDlpRequiresBypassJustification;
using test::NormalSettingsWithCustomMessage;
using test::NoSettings;
using test::OnlyDlpEnabledSettings;
using test::TestParam;

class AnalysisServiceSettingsCloud : public AnalysisServiceSettingsBase {
 public:
  explicit AnalysisServiceSettingsCloud(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config)
      : AnalysisServiceSettingsBase(settings_value, service_provider_config) {}
};

}  // namespace

class AnalysisServiceSettingsCloudTest
    : public testing::TestWithParam<TestParam> {
 public:
  GURL url() const { return GURL(GetParam().url); }
  std::string GetSettingsValue() const {
    std::string value = GetParam().settings_value;
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "%s", "google");
    // no additional settings needed
    base::ReplaceFirstSubstringAfterOffset(&value, 0, "%s", "");
    return value;
  }
  AnalysisSettings* expected_settings() const {
    // Set the GURL field dynamically to avoid static initialization issues.
    if (GetParam().expected_settings != NoSettings()) {
      GURL regionalized_url =
          GURL(GetServiceProviderConfig()
                   ->at("google")
                   .analysis->region_urls[static_cast<size_t>(data_region())]);
      CloudAnalysisSettings cloud_settings;
      cloud_settings.analysis_url = regionalized_url;
      GetParam().expected_settings->cloud_or_local_settings =
          CloudOrLocalAnalysisSettings(std::move(cloud_settings));
    }

    return GetParam().expected_settings;
  }
  DataRegion data_region() const { return GetParam().data_region; }
};

TEST_P(AnalysisServiceSettingsCloudTest, CloudTest) {
  auto settings = base::JSONReader::Read(GetSettingsValue(),
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  AnalysisServiceSettingsCloud service_settings(settings.value(),
                                                *GetServiceProviderConfig());

  auto analysis_settings =
      service_settings.GetAnalysisSettings(url(), data_region());
  ASSERT_EQ((expected_settings() != nullptr), analysis_settings.has_value());
  if (analysis_settings.has_value()) {
    ASSERT_EQ(analysis_settings.value().block_until_verdict,
              expected_settings()->block_until_verdict);
    ASSERT_EQ(analysis_settings.value().default_action,
              expected_settings()->default_action);
    ASSERT_EQ(analysis_settings.value().block_password_protected_files,
              expected_settings()->block_password_protected_files);
    ASSERT_EQ(analysis_settings.value().block_large_files,
              expected_settings()->block_large_files);
    ASSERT_TRUE(
        analysis_settings.value().cloud_or_local_settings.is_cloud_analysis());
    ASSERT_EQ(analysis_settings.value().cloud_or_local_settings.analysis_url(),
              expected_settings()->cloud_or_local_settings.analysis_url());
    ASSERT_EQ(analysis_settings.value().minimum_data_size,
              expected_settings()->minimum_data_size);
    for (const auto& [tag, expected_tag_settings] : expected_settings()->tags) {
      const auto& tag_settings_it = analysis_settings.value().tags.find(tag);
      ASSERT_NE(analysis_settings.value().tags.end(), tag_settings_it);

      const auto& tag_settings = tag_settings_it->second;
      ASSERT_EQ(tag_settings.custom_message.message,
                expected_tag_settings.custom_message.message);
      if (!tag_settings.custom_message.learn_more_url.is_empty()) {
        const auto& learn_more_url_spec =
            GetExpectedLearnMoreUrlSpecs().at(tag);
        ASSERT_EQ(learn_more_url_spec,
                  tag_settings.custom_message.learn_more_url.spec());
        ASSERT_EQ(learn_more_url_spec,
                  service_settings.GetLearnMoreUrl(tag).value().spec());
      }
      ASSERT_EQ(tag_settings.requires_justification,
                expected_tag_settings.requires_justification);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AnalysisServiceSettingsCloudTest,
    testing::Values(
        // Validate that the enabled patterns match the expected patterns.
        TestParam(kScan1DotCom,
                  kOnlyDlpEnabledPatternsSettings,
                  OnlyDlpEnabledSettings()),
        TestParam(kScan2DotCom,
                  kOnlyDlpEnabledPatternsSettings,
                  OnlyDlpEnabledSettings()),
        TestParam(kNoDlpDotCom, kOnlyDlpEnabledPatternsSettings, NoSettings()),
        TestParam(kNoMalwareDotCom,
                  kOnlyDlpEnabledPatternsSettings,
                  NoSettings()),
        TestParam(kNoDlpOrMalwareDotCa,
                  kOnlyDlpEnabledPatternsSettings,
                  NoSettings()),

        // Validate that invalid enable list entries are ignored.
        TestParam(kScan1DotCom, kEnablePatternIsNotADictSettings, NoSettings()),
        TestParam(kScan2DotCom, kEnablePatternIsNotADictSettings, NoSettings()),
        TestParam(kScan1DotCom,
                  kOnlyDlpEnabledPatternsAndIrrelevantSettings,
                  OnlyDlpEnabledSettings()),
        TestParam(kScan2DotCom,
                  kOnlyDlpEnabledPatternsAndIrrelevantSettings,
                  OnlyDlpEnabledSettings()),

        // kUrlAndSourceDestinationListSettings is invalid and should result in
        // no settings.
        TestParam(kScan1DotCom,
                  kUrlAndSourceDestinationListSettings,
                  NoSettings()),
        TestParam(kScan2DotCom,
                  kUrlAndSourceDestinationListSettings,
                  NoSettings()),

        // Validate that each URL gets the correct tag on the normal settings.
        TestParam(kScan1DotCom, kNormalSettings, NormalDlpAndMalwareSettings()),
        TestParam(kScan2DotCom, kNormalSettings, NoSettings()),
        TestParam(kNoDlpDotCom, kNormalSettings, NormalMalwareSettings()),
        TestParam(kNoMalwareDotCom, kNormalSettings, NormalDlpSettings()),
        TestParam(kNoDlpOrMalwareDotCa, kNormalSettings, NoSettings()),

        // Validate that each URL gets no settings when either the provider is
        // absent or when there are no enabled patterns.
        TestParam(kScan1DotCom, kNoProviderSettings, NoSettings()),
        TestParam(kScan2DotCom, kNoProviderSettings, NoSettings()),
        TestParam(kNoDlpDotCom, kNoProviderSettings, NoSettings()),
        TestParam(kNoMalwareDotCom, kNoProviderSettings, NoSettings()),
        TestParam(kNoDlpOrMalwareDotCa, kNoProviderSettings, NoSettings()),

        TestParam(kScan1DotCom, kNoEnabledPatternsSettings, NoSettings()),
        TestParam(kScan2DotCom, kNoEnabledPatternsSettings, NoSettings()),
        TestParam(kNoDlpDotCom, kNoEnabledPatternsSettings, NoSettings()),
        TestParam(kNoMalwareDotCom, kNoEnabledPatternsSettings, NoSettings()),
        TestParam(kNoDlpOrMalwareDotCa,
                  kNoEnabledPatternsSettings,
                  NoSettings()),

        // Validate custom messages and bypass justifications.
        TestParam(kScan1DotCom,
                  kNormalSettingsWithCustomMessage,
                  NormalSettingsWithCustomMessage()),
        TestParam(kScan1DotCom,
                  kNormalSettingsDlpRequiresBypassJustification,
                  NormalSettingsDlpRequiresBypassJustification()),

        // Validate regionalized endpoints.
        TestParam(kScan1DotCom,
                  kNormalSettings,
                  NormalDlpSettings(),
                  DataRegion::UNITED_STATES),
        TestParam(kScan1DotCom,
                  kNormalSettings,
                  NormalDlpSettings(),
                  DataRegion::EUROPE)));

}  //  namespace enterprise_connectors
