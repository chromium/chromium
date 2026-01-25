// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"

#include <map>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kNavigationCount[] = "navigation_count";
constexpr char kContentTransferCount[] = "content_transfer_count";
constexpr char kEncryptionProtocols[] = "encryption_protocols";
}  // namespace

namespace enterprise_reporting {

enum class ActionType { kNavigation, kContentTransfer };

struct Action {
  ActionType type;
  std::string domain;
  std::string protocol;
};

struct ExpectedDomainState {
  int navigation_count = 0;
  int content_transfer_count = 0;
  std::vector<std::string> encryption_protocols;
};

struct TestCase {
  std::string test_name;
  std::vector<Action> actions;
  std::map<std::string, ExpectedDomainState> expected_states;
};

class DomainReportingAggregationUtilsParameterizedTest
    : public testing::Test,
      public testing::WithParamInterface<TestCase> {
 public:
  DomainReportingAggregationUtilsParameterizedTest() = default;
  ~DomainReportingAggregationUtilsParameterizedTest() override = default;

  void SetUp() override {
    pref_service_.registry()->RegisterDictionaryPref(kSaasUsageReport);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_P(DomainReportingAggregationUtilsParameterizedTest, Run) {
  const TestCase& test_case = GetParam();

  for (const auto& action : test_case.actions) {
    if (action.type == ActionType::kNavigation) {
      enterprise_reporting::RecordNavigation(&pref_service_, action.domain,
                                             action.protocol);
    } else {
      enterprise_reporting::RecordContentTransfer(&pref_service_,
                                                  action.domain);
    }
  }

  const base::DictValue& report = pref_service_.GetDict(kSaasUsageReport);
  ASSERT_EQ(test_case.expected_states.size(), report.size());

  for (const auto& [domain, expected] : test_case.expected_states) {
    const base::DictValue* entry = report.FindDict(domain);
    ASSERT_TRUE(entry) << "Report for domain " << domain << " not found.";

    EXPECT_EQ(expected.navigation_count,
              entry->FindInt(kNavigationCount).value_or(0));
    EXPECT_EQ(expected.content_transfer_count,
              entry->FindInt(kContentTransferCount).value_or(0));

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
    DomainReportingAggregationUtilsParameterizedTest,
    testing::Values(
        TestCase{"RecordNavigation_NewDomain",
                 {{ActionType::kNavigation, "example.com", "TLS 1.3"}},
                 {{"example.com", {1, 0, {"TLS 1.3"}}}}},
        TestCase{"RecordNavigation_ExistingDomain",
                 {{ActionType::kNavigation, "example.com", "TLS 1.3"},
                  {ActionType::kNavigation, "example.com", "TLS 1.3"}},
                 {{"example.com", {2, 0, {"TLS 1.3"}}}}},
        TestCase{"RecordNavigation_NewProtocol",
                 {{ActionType::kNavigation, "example.com", "TLS 1.2"},
                  {ActionType::kNavigation, "example.com", "TLS 1.3"}},
                 {{"example.com", {2, 0, {"TLS 1.2", "TLS 1.3"}}}}},
        TestCase{"RecordNavigation_ExistingProtocol",
                 {{ActionType::kNavigation, "example.com", "TLS 1.2"},
                  {ActionType::kNavigation, "example.com", "TLS 1.3"},
                  {ActionType::kNavigation, "example.com", "TLS 1.3"}},
                 {{"example.com", {3, 0, {"TLS 1.2", "TLS 1.3"}}}}},
        TestCase{"RecordNavigation_EmptyProtocol",
                 {{ActionType::kNavigation, "example.com", ""}},
                 {{"example.com", {1, 0, {}}}}},
        TestCase{"RecordNavigation_MultipleDomains",
                 {{ActionType::kNavigation, "example.com", "TLS 1.3"},
                  {ActionType::kNavigation, "google.com", "QUIC"}},
                 {{"example.com", {1, 0, {"TLS 1.3"}}},
                  {"google.com", {1, 0, {"QUIC"}}}}},
        TestCase{"RecordContentTransfer_NewDomain",
                 {{ActionType::kContentTransfer, "example.com"}},
                 {{"example.com", {0, 1, {}}}}},
        TestCase{"RecordContentTransfer_ExistingDomain",
                 {{ActionType::kContentTransfer, "example.com"},
                  {ActionType::kContentTransfer, "example.com"}},
                 {{"example.com", {0, 2, {}}}}},
        TestCase{"RecordContentTransfer_MultipleDomains",
                 {{ActionType::kContentTransfer, "example.com"},
                  {ActionType::kContentTransfer, "google.com"}},
                 {{"example.com", {0, 1, {}}}, {"google.com", {0, 1, {}}}}},
        TestCase{"RecordContentTransfer_WithNavigation",
                 {{ActionType::kNavigation, "example.com", "TLS 1.3"},
                  {ActionType::kContentTransfer, "example.com"}},
                 {{"example.com", {1, 1, {"TLS 1.3"}}}}}),
    [](const testing::TestParamInfo<
        DomainReportingAggregationUtilsParameterizedTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace enterprise_reporting
