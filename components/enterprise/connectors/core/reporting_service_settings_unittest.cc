// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_service_settings.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kNoProviderSettings[] = "{}";

constexpr char kNormalSettingsWithoutEvents[] =
    R"({ "service_provider": "google" })";

constexpr char kNormalSettingsWithEvents[] =
    R"({ "service_provider": "google",
         "enabled_event_names" : ["event 1", "event 2", "event 3"]
       })";

constexpr char kNormalSettingsWithOptInEvents[] =
    R"({ "service_provider": "google",
         "enabled_opt_in_events" : [
            { "name": "opt_in_event 1", "url_patterns" : []},
            { "name": "opt_in_event 2", "url_patterns" : ["*"]},
            {
              "name": "opt_in_event 3",
              "url_patterns" : ["example.com", "other.example.com"]
            }
          ]})";

}  // namespace

class ReportingServiceSettingsTest : public testing::Test {
 public:
  std::optional<ReportingSettings> GetReportingSettings(
      const char* settings_value) {
    auto settings = base::JSONReader::Read(settings_value,
                                           base::JSON_ALLOW_TRAILING_COMMAS);
    EXPECT_TRUE(settings.has_value());

    ReportingServiceSettings service_settings(settings.value(),
                                              *GetServiceProviderConfig());

    return service_settings.GetReportingSettings();
  }
};

TEST_F(ReportingServiceSettingsTest, TestNoSettings) {
  std::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNoProviderSettings);
  ASSERT_FALSE(reporting_settings.has_value());
}

TEST_F(ReportingServiceSettingsTest, TestNormalSettingsWithoutEvents) {
  std::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNormalSettingsWithoutEvents);
  ASSERT_TRUE(reporting_settings.has_value());
}

TEST_F(ReportingServiceSettingsTest, TestNormalSettingsWithEvents) {
  std::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNormalSettingsWithEvents);
  ASSERT_TRUE(reporting_settings.has_value());

  ASSERT_FALSE(reporting_settings->enabled_event_names.empty());
  std::set<std::string> expected_event_names{"event 1", "event 2", "event 3"};
  ASSERT_EQ(expected_event_names,
            reporting_settings.value().enabled_event_names);
}

TEST_F(ReportingServiceSettingsTest, TestNormalSettingsWithOptInEvents) {
  std::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNormalSettingsWithOptInEvents);
  ASSERT_TRUE(reporting_settings.has_value());

  std::map<std::string, std::vector<std::string>> actual_opt_in_events =
      reporting_settings.value().enabled_opt_in_events;
  ASSERT_EQ(2UL, actual_opt_in_events.size());

  // An event with no URL patterns isn't enabled.
  ASSERT_EQ(actual_opt_in_events.find("opt_in_event 1"),
            actual_opt_in_events.end());

  ASSERT_NE(actual_opt_in_events.find("opt_in_event 2"),
            actual_opt_in_events.end());
  ASSERT_EQ(1UL, actual_opt_in_events["opt_in_event 2"].size());

  ASSERT_NE(actual_opt_in_events.find("opt_in_event 3"),
            actual_opt_in_events.end());
  ASSERT_EQ(2UL, actual_opt_in_events["opt_in_event 3"].size());
}

}  // namespace enterprise_connectors
