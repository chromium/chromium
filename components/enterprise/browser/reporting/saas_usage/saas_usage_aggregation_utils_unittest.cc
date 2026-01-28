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
constexpr char kEncryptionProtocols[] = "encryption_protocols";
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
    enterprise_reporting::RecordNavigation(&pref_service_, navigation.domain,
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

}  // namespace enterprise_reporting
