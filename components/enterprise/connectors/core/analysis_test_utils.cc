// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/analysis_test_utils.h"

#include "base/no_destructor.h"

namespace enterprise_connectors::test {

namespace {

AnalysisSettings NormalSettingsWithTags(
    std::map<std::string, TagSettings> tags) {
  AnalysisSettings settings;
  settings.tags = std::move(tags);
  settings.block_until_verdict = BlockUntilVerdict::kBlock;
  settings.default_action = DefaultAction::kBlock;
  settings.block_password_protected_files = true;
  settings.block_large_files = true;
  settings.minimum_data_size = 123;
  return settings;
}

}  // namespace

// The JSON settings strings below typically use two '%s' placeholders.
// The second '%s' (often on its own line) is for injecting additional JSON
// fields, such as a "verification" field used in local analysis service tests.
const char kNormalSettings[] = R"({
  "service_provider": "%s",
  %s
  "enable": [
    {"url_list": ["*"], "tags": ["dlp", "malware"]},
  ],
  "disable": [
    {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
         "tags": ["malware"]},
    {"url_list": ["scan2.com"], "tags": ["dlp", "malware"]},
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
  "minimum_data_size": 123,
})";

const char kOnlyDlpEnabledPatternsSettings[] = R"({
  "service_provider": "%s",
  %s
  "enable": [
    {"url_list": ["scan1.com", "scan2.com"], "tags": ["dlp"]},
  ],
})";

const char kEnablePatternIsNotADictSettings[] = R"({
  "service_provider": "%s",
  "enable": [
    "url_list",
  ],
})";

const char kOnlyDlpEnabledPatternsAndIrrelevantSettings[] = R"({
  "service_provider": "%s",
  %s
  "enable": [
    {"tags": ["dlp", "malware"]},
    {"url_list": ["scan1.com", "scan2.com"], "tags": ["dlp"]},
    {"url_list": [], "tags": ["malware"]},
  ],
})";

const char kUrlAndSourceDestinationListSettings[] =
    R"({
  "service_provider": "%s",
  "enable": [
    {
      "url_list": ["scan1.com", "scan2.com"],
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "ANY"
          }],
          "destinations": [{
            "file_system_type": "ANY"
          }]
        }
      ],
      "tags": ["dlp"]
    },
  ],
})";

// This string has a dummy field so that the service provider name is filled
// in there and does not get set in the "service_provider" field.  This is
// needed for the base::StringPrintf() in settings_value() to work correctly.
const char kNoProviderSettings[] = R"({
  "dummy": "%s",
  %s
  "enable": [
    {"url_list": ["*"], "tags": ["dlp", "malware"]},
  ],
  "disable": [
    {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
         "tags": ["malware"]},
    {"url_list": ["scan2.com"], "tags": ["dlp", "malware"]},
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
  "minimum_data_size": 123,
})";

const char kNoEnabledPatternsSettings[] = R"({
  "service_provider": "%s",
  %s
  "disable": [
    {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
         "tags": ["malware"]},
    {"url_list": ["scan2.com"], "tags": ["dlp", "malware"]},
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
})";

const char kNormalSettingsWithCustomMessage[] = R"({
  "service_provider": "%s",
  %s
  "enable": [
    {"url_list": ["*"], "tags": ["dlp", "malware"]},
  ],
  "disable": [
    {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
         "tags": ["malware"]},
    {"url_list": ["scan2.com"], "tags": ["dlp", "malware"]},
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
  "minimum_data_size": 123,
  "custom_messages": [
    {
      "message": "dlpabcèéç",
      "learn_more_url": "http://www.example.com/dlp",
      "tag": "dlp"
    },
    {
      "message": "malwareabcèéç",
      "learn_more_url": "http://www.example.com/malware",
      "tag": "malware"
    },
  ],
})";

const char kNormalSettingsDlpRequiresBypassJustification[] = R"({
  "service_provider": "%s",
  %s
  "enable": [
    {"url_list": ["*"], "tags": ["dlp", "malware"]},
  ],
  "disable": [
    {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
         "tags": ["malware"]},
    {"url_list": ["scan2.com"], "tags": ["dlp", "malware"]},
  ],
  "block_until_verdict": 1,
  "default_action": "block",
  "block_password_protected": true,
  "block_large_files": true,
  "minimum_data_size": 123,
  "require_justification_tags": ["dlp"],
})";

const char kScan1DotCom[] = "https://scan1.com";
const char kScan2DotCom[] = "https://scan2.com";
const char kNoDlpDotCom[] = "https://no.dlp.com";
const char kNoMalwareDotCom[] = "https://no.malware.com";
const char kNoDlpOrMalwareDotCa[] = "https://no.dlp.or.malware.ca";

// These URLs can't be added directly to the "expected" settings object, because
// it's created statically and statically initializing GURLs is prohibited.
const std::map<std::string, std::string>& GetExpectedLearnMoreUrlSpecs() {
  static const base::NoDestructor<std::map<std::string, std::string>>
      kExpectedLearnMoreUrlSpecs{
          {{"dlp", "http://www.example.com/dlp"},
           {"malware", "http://www.example.com/malware"}},
      };
  return *kExpectedLearnMoreUrlSpecs;
}

// TODO(crbug.com/452865019): Test utils and tests use raw ptr because
// AnalysisSettings is not copyable, switching to std::optional requires an
// overhaul
AnalysisSettings* OnlyDlpEnabledSettings() {
  static base::NoDestructor<AnalysisSettings> settings([]() {
    AnalysisSettings settings;
    settings.tags = {{"dlp", TagSettings()}};
    return settings;
  }());
  return settings.get();
}

AnalysisSettings* NormalDlpSettings() {
  static base::NoDestructor<AnalysisSettings> settings(
      NormalSettingsWithTags({{"dlp", TagSettings()}}));
  return settings.get();
}

AnalysisSettings* NormalMalwareSettings() {
  static base::NoDestructor<AnalysisSettings> settings(
      NormalSettingsWithTags({{"malware", TagSettings()}}));
  return settings.get();
}

AnalysisSettings* NormalDlpAndMalwareSettings() {
  static base::NoDestructor<AnalysisSettings> settings(NormalSettingsWithTags(
      {{"dlp", TagSettings()}, {"malware", TagSettings()}}));
  return settings.get();
}

AnalysisSettings* NormalSettingsWithCustomMessage() {
  static base::NoDestructor<AnalysisSettings> settings([]() {
    AnalysisSettings settings = NormalSettingsWithTags({
        {
            "dlp",
            {
                .custom_message =
                    {
                        .message = u"dlpabcèéç",
                    },
            },
        },
        {
            "malware",
            {
                .custom_message =
                    {
                        .message = u"malwareabcèéç",
                    },
            },
        },
    });
    return settings;
  }());
  return settings.get();
}

AnalysisSettings* NormalSettingsDlpRequiresBypassJustification() {
  static base::NoDestructor<AnalysisSettings> settings([]() {
    AnalysisSettings settings = NormalSettingsWithTags({
        {
            "dlp",
            {
                .requires_justification = true,
            },
        },
        {"malware", TagSettings()},
    });
    return settings;
  }());
  return settings.get();
}

AnalysisSettings* NoSettings() {
  return nullptr;
}

}  // namespace enterprise_connectors::test
