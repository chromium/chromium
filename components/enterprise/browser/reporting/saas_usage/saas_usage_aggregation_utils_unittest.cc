// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"

#include <map>
#include <string>
#include <vector>

#include "base/json/values_util.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kNavigationCount[] = "navigation_count";
constexpr char kEncryptionProtocols[] = "encryption_protocols";
constexpr char kFirstSeenTime[] = "first_seen_time";
constexpr char kLastSeenTime[] = "last_seen_time";
}  // namespace

namespace enterprise_reporting {

struct Navigation {
  std::string domain;
  std::string protocol;
};

struct ExpectedDomainState {
  int navigation_count = 0;
  std::vector<std::string> encryption_protocols;
};

struct TestCase {
  std::string test_name;
  std::vector<Navigation> navigations;
  std::map<std::string, ExpectedDomainState> expected_states;
};

class SaasUsageAggregationUtilsParameterizedTest
    : public testing::Test,
      public testing::WithParamInterface<TestCase> {
 public:
  SaasUsageAggregationUtilsParameterizedTest() = default;
  ~SaasUsageAggregationUtilsParameterizedTest() override = default;

  void SetUp() override {
    pref_service_.registry()->RegisterDictionaryPref(kSaasUsageReport);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_P(SaasUsageAggregationUtilsParameterizedTest, Run) {
  const TestCase& test_case = GetParam();

  for (const auto& navigation : test_case.navigations) {
    enterprise_reporting::RecordNavigation(pref_service_, navigation.domain,
                                           navigation.protocol);
  }

  const base::DictValue& report = pref_service_.GetDict(kSaasUsageReport);
  ASSERT_EQ(test_case.expected_states.size(), report.size());

  for (const auto& [domain, expected] : test_case.expected_states) {
    const base::DictValue* entry = report.FindDict(domain);
    ASSERT_TRUE(entry) << "Report for domain " << domain << " not found.";

    EXPECT_EQ(expected.navigation_count,
              entry->FindInt(kNavigationCount).value_or(0));

    const base::ListValue* protocols = entry->FindList(kEncryptionProtocols);
    ASSERT_TRUE(protocols);
    EXPECT_EQ(expected.encryption_protocols.size(), protocols->size());
    for (const auto& protocol : expected.encryption_protocols) {
      EXPECT_TRUE(protocols->contains(protocol))
          << "Protocol " << protocol << " missing for domain " << domain;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SaasUsageAggregationUtilsParameterizedTest,
    testing::Values(
        TestCase{"RecordNavigation_NewDomain",
                 {{"example.com", "TLS 1.3"}},
                 {{"example.com", {1, {"TLS 1.3"}}}}},
        TestCase{"RecordNavigation_ExistingDomain",
                 {{"example.com", "TLS 1.3"}, {"example.com", "TLS 1.3"}},
                 {{"example.com", {2, {"TLS 1.3"}}}}},
        TestCase{"RecordNavigation_NewProtocol",
                 {{"example.com", "TLS 1.2"}, {"example.com", "TLS 1.3"}},
                 {{"example.com", {2, {"TLS 1.2", "TLS 1.3"}}}}},
        TestCase{"RecordNavigation_ExistingProtocol",
                 {{"example.com", "TLS 1.2"},
                  {"example.com", "TLS 1.3"},
                  {"example.com", "TLS 1.3"}},
                 {{"example.com", {3, {"TLS 1.2", "TLS 1.3"}}}}},
        TestCase{"RecordNavigation_EmptyProtocol",
                 {{"example.com", ""}},
                 {{"example.com", {1, {}}}}},
        TestCase{"RecordNavigation_MultipleDomains",
                 {{"example.com", "TLS 1.3"}, {"google.com", "QUIC"}},
                 {{"example.com", {1, {"TLS 1.3"}}},
                  {"google.com", {1, {"QUIC"}}}}}),
    [](const testing::TestParamInfo<
        SaasUsageAggregationUtilsParameterizedTest::ParamType>& info) {
      return info.param.test_name;
    });

class SaasUsageAggregationUtilsTest : public testing::Test {
 public:
  SaasUsageAggregationUtilsTest() = default;
  ~SaasUsageAggregationUtilsTest() override = default;

  void SetUp() override {
    pref_service_.registry()->RegisterDictionaryPref(kSaasUsageReport);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SaasUsageAggregationUtilsTest, FirstAndLastSeenTime) {
  const std::string domain = "example.com";
  const base::Time first_seen_time = base::Time::Now();

  // First navigation, start and end time should be the same.
  RecordNavigation(pref_service_, domain, "TLS 1.3");

  const base::DictValue& report = pref_service_.GetDict(kSaasUsageReport);
  const base::DictValue* entry = report.FindDict(domain);
  ASSERT_TRUE(entry);

  const base::Value* first_seen_time_value = entry->Find(kFirstSeenTime);
  ASSERT_TRUE(first_seen_time_value);
  EXPECT_EQ(first_seen_time, base::ValueToTime(first_seen_time_value).value());

  const base::Value* last_seen_time_value = entry->Find(kLastSeenTime);
  ASSERT_TRUE(last_seen_time_value);
  EXPECT_EQ(first_seen_time, base::ValueToTime(last_seen_time_value).value());

  // Advance time and record another navigation.
  task_environment_.FastForwardBy(base::Seconds(10));
  const base::Time last_seen_time = base::Time::Now();
  RecordNavigation(pref_service_, domain, "TLS 1.3");

  // Start time should be unchanged, end time should be updated.
  first_seen_time_value = entry->Find(kFirstSeenTime);
  ASSERT_TRUE(first_seen_time_value);
  EXPECT_EQ(first_seen_time, base::ValueToTime(first_seen_time_value).value());

  last_seen_time_value = entry->Find(kLastSeenTime);
  ASSERT_TRUE(last_seen_time_value);
  EXPECT_EQ(last_seen_time, base::ValueToTime(last_seen_time_value).value());
}

TEST_F(SaasUsageAggregationUtilsTest, PopulateSaasUsageDomainMetrics) {
  // Record some navigations.
  const base::Time example_start_time = base::Time::Now();
  RecordNavigation(pref_service_, "example.com", "TLS 1.3");
  task_environment_.FastForwardBy(base::Seconds(5));
  const base::Time example_end_time = base::Time::Now();
  RecordNavigation(pref_service_, "example.com", "TLS 1.2");

  task_environment_.FastForwardBy(base::Seconds(10));
  const base::Time google_start_time = base::Time::Now();
  RecordNavigation(pref_service_, "google.com", "QUIC");
  task_environment_.FastForwardBy(base::Seconds(13));
  const base::Time google_end_time = base::Time::Now();
  RecordNavigation(pref_service_, "google.com", "QUIC");

  // Create the report.
  ::chrome::cros::reporting::proto::SaasUsageReportEvent report;
  PopulateSaasUsageDomainMetrics(pref_service_, report);

  ::chrome::cros::reporting::proto::SaasUsageReportEvent expected_report;
  auto* example_event = expected_report.add_domain_metrics();
  example_event->set_domain("example.com");
  example_event->set_visit_count(2);
  example_event->set_start_time_millis(
      example_start_time.InMillisecondsSinceUnixEpoch());
  example_event->set_end_time_millis(
      example_end_time.InMillisecondsSinceUnixEpoch());
  example_event->add_encryption_protocols("TLS 1.3");
  example_event->add_encryption_protocols("TLS 1.2");

  auto* google_event = expected_report.add_domain_metrics();
  google_event->set_domain("google.com");
  google_event->set_visit_count(2);
  google_event->set_start_time_millis(
      google_start_time.InMillisecondsSinceUnixEpoch());
  google_event->set_end_time_millis(
      google_end_time.InMillisecondsSinceUnixEpoch());
  google_event->add_encryption_protocols("QUIC");

  EXPECT_THAT(
      report.domain_metrics(),
      testing::UnorderedElementsAre(base::test::EqualsProto(*example_event),
                                    base::test::EqualsProto(*google_event)));
}

TEST_F(SaasUsageAggregationUtilsTest,
       PopulateSaasUsageDomainMetrics_EmptyReport) {
  ::chrome::cros::reporting::proto::SaasUsageReportEvent report;
  PopulateSaasUsageDomainMetrics(pref_service_, report);
  EXPECT_TRUE(report.domain_metrics().empty());
}

TEST_F(SaasUsageAggregationUtilsTest,
       PopulateSaasUsageDomainMetrics_CorruptedEntry_Skipped) {
  const base::DictValue dict =
      base::DictValue().Set("example.com", base::Value(1));
  pref_service_.SetDict(kSaasUsageReport, dict.Clone());
  ::chrome::cros::reporting::proto::SaasUsageReportEvent report;
  PopulateSaasUsageDomainMetrics(pref_service_, report);
  EXPECT_TRUE(report.domain_metrics().empty());
}

TEST_F(SaasUsageAggregationUtilsTest,
       PopulateSaasUsageDomainMetrics_CorruptedTimeEntry_Skipped) {
  const base::DictValue dict = base::DictValue().Set(
      "example.com", base::DictValue()
                         .Set(kNavigationCount, 1)
                         .Set(kEncryptionProtocols, base::ListValue())
                         .Set(kFirstSeenTime, base::Value("invalid_time"))
                         .Set(kLastSeenTime, base::Value("invalid_time")));
  pref_service_.SetDict(kSaasUsageReport, dict.Clone());
  ::chrome::cros::reporting::proto::SaasUsageReportEvent report;
  PopulateSaasUsageDomainMetrics(pref_service_, report);
  EXPECT_EQ(0, report.domain_metrics_size());
}

TEST_F(
    SaasUsageAggregationUtilsTest,
    PopulateSaasUsageDomainMetrics_CorruptedEncryptionProtocolsEntry_Skipped) {
  base::DictValue dict = base::DictValue().Set(
      "example.com",
      base::DictValue()
          .Set(kNavigationCount, 1)
          .Set(kEncryptionProtocols, base::Value(1))
          .Set(kFirstSeenTime, base::TimeToValue(base::Time::Now()))
          .Set(kLastSeenTime, base::TimeToValue(base::Time::Now())));
  pref_service_.SetDict(kSaasUsageReport, std::move(dict));
  ::chrome::cros::reporting::proto::SaasUsageReportEvent report;
  PopulateSaasUsageDomainMetrics(pref_service_, report);
  EXPECT_EQ(0, report.domain_metrics_size());
}

TEST_F(SaasUsageAggregationUtilsTest, ClearSaasUsageReport) {
  RecordNavigation(pref_service_, "example.com", "TLS 1.3");
  EXPECT_FALSE(pref_service_.GetDict(kSaasUsageReport).empty());

  ClearSaasUsageReport(pref_service_);
  EXPECT_TRUE(pref_service_.GetDict(kSaasUsageReport).empty());
}

}  // namespace enterprise_reporting
