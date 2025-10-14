// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_TEST_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"

namespace enterprise_connectors::test {

// Test parameter struct for analysis settings with regionalized url.
struct TestParam {
  TestParam(const char* url,
            const char* settings_value,
            AnalysisSettings* expected_settings,
            DataRegion data_region = DataRegion::NO_PREFERENCE)
      : url(url),
        settings_value(settings_value),
        expected_settings(expected_settings),
        data_region(data_region) {}
  ~TestParam() = default;
  TestParam(const TestParam&) = default;

  const char* url;
  const char* settings_value;
  raw_ptr<AnalysisSettings> expected_settings;
  DataRegion data_region;
};

// Settings strings for tests.
extern const char kNormalSettings[];
extern const char kOnlyDlpEnabledPatternsSettings[];
extern const char kEnablePatternIsNotADictSettings[];
extern const char kOnlyDlpEnabledPatternsAndIrrelevantSettings[];
extern const char kUrlAndSourceDestinationListSettings[];
extern const char kNoProviderSettings[];
extern const char kNoEnabledPatternsSettings[];
extern const char kNormalSettingsWithCustomMessage[];
extern const char kNormalSettingsDlpRequiresBypassJustification[];

// URLs for tests.
extern const char kScan1DotCom[];
extern const char kScan2DotCom[];
extern const char kNoDlpDotCom[];
extern const char kNoMalwareDotCom[];
extern const char kNoDlpOrMalwareDotCa[];

// Expected learn more URLs.
const std::map<std::string, std::string>& GetExpectedLearnMoreUrlSpecs();

// Helper functions to get expected AnalysisSettings.
AnalysisSettings* OnlyDlpEnabledSettings();
AnalysisSettings* NormalDlpSettings();
AnalysisSettings* NormalMalwareSettings();
AnalysisSettings* NormalDlpAndMalwareSettings();
AnalysisSettings* NormalSettingsWithCustomMessage();
AnalysisSettings* NormalSettingsDlpRequiresBypassJustification();
AnalysisSettings* NoSettings();

}  // namespace enterprise_connectors::test

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_TEST_UTILS_H_
