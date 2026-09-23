// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_internals_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "components/enterprise/device_trust/core/common_types.h"  // nogncheck

#if BUILDFLAG(ENTERPRISE_PROXY)
#include "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#endif

namespace enterprise_connectors::utils {

namespace {

#if BUILDFLAG(ENTERPRISE_PROXY)
using ::testing::NiceMock;
using ::testing::Return;

base::DictValue CreatePvdPolicy(const std::string& pvd_id) {
  base::DictValue policy;
  policy.Set("pvd_id", pvd_id);
  return policy;
}

base::DictValue CreateFetchedConfig(const std::string& identifier) {
  base::DictValue config;
  config.Set("identifier", identifier);
  return config;
}
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

class ConnectorsInternalsUtilsTest : public testing::Test {
 public:
  ConnectorsInternalsUtilsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ConnectorsInternalsUtilsTest, GetStringFromTimestamp) {
  EXPECT_EQ(GetStringFromTimestamp(base::Time()), std::string());

  base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  EXPECT_FALSE(GetStringFromTimestamp(time).empty());
}

TEST_F(ConnectorsInternalsUtilsTest, ConvertPolicyLevelToString) {
  EXPECT_EQ(ConvertPolicyLevelToString(DTCPolicyLevel::kBrowser),
            kBrowserLevel);
  EXPECT_EQ(ConvertPolicyLevelToString(DTCPolicyLevel::kUser), kUserLevel);
}

TEST_F(ConnectorsInternalsUtilsTest, GetPolicyEnabledLevels_NullService) {
  EXPECT_TRUE(GetPolicyEnabledLevels(nullptr).empty());
}

TEST_F(ConnectorsInternalsUtilsTest, CreateUnsupportedDeviceTrustState) {
  auto state = CreateUnsupportedDeviceTrustState();
  ASSERT_TRUE(state);
  EXPECT_FALSE(state->is_enabled);
  EXPECT_TRUE(state->policy_enabled_levels.empty());
  ASSERT_TRUE(state->key_info);
  EXPECT_EQ(
      state->key_info->is_key_manager_initialized,
      connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED);
  EXPECT_TRUE(state->signals_json.empty());
  EXPECT_FALSE(state->consent_metadata);
}

TEST_F(ConnectorsInternalsUtilsTest, CreateDeviceTrustStateWithNoKey) {
  std::vector<std::string> levels = {"Browser"};
  std::string signals_json = "{\"test\": 1}";
  auto state = CreateDeviceTrustStateWithNoKey(
      /*is_device_trust_enabled=*/true, levels, signals_json,
      /*consent_metadata=*/nullptr);
  ASSERT_TRUE(state);
  EXPECT_TRUE(state->is_enabled);
  EXPECT_EQ(state->policy_enabled_levels, levels);
  ASSERT_TRUE(state->key_info);
  EXPECT_EQ(state->key_info->is_key_manager_initialized,
            connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY);
  EXPECT_EQ(state->signals_json, signals_json);
  EXPECT_FALSE(state->consent_metadata);
}

TEST_F(ConnectorsInternalsUtilsTest, CreateSignalsReportingState_WithPrefs) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterTimePref(
      enterprise_reporting::kLastSignalsUploadAttemptTimestamp, base::Time());
  prefs.registry()->RegisterTimePref(
      enterprise_reporting::kLastSignalsUploadSucceededTimestamp, base::Time());
  prefs.registry()->RegisterStringPref(
      enterprise_reporting::kLastSignalsUploadSucceededConfig, std::string());

  base::Time attempt_time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  base::Time success_time = base::Time::FromSecondsSinceUnixEpoch(1700000100);
  prefs.SetTime(enterprise_reporting::kLastSignalsUploadAttemptTimestamp,
                attempt_time);
  prefs.SetTime(enterprise_reporting::kLastSignalsUploadSucceededTimestamp,
                success_time);
  prefs.SetString(enterprise_reporting::kLastSignalsUploadSucceededConfig,
                  "test_config");

  auto state =
      CreateSignalsReportingState(&prefs, /*report_scheduler=*/nullptr,
                                  /*can_collect_all_signals=*/true,
                                  /*error_info=*/"Scheduler not ready");

  ASSERT_TRUE(state);
  ASSERT_TRUE(state->error_info.has_value());
  EXPECT_EQ(*state->error_info, "Scheduler not ready");
  EXPECT_FALSE(state->status_report_enabled);
  EXPECT_FALSE(state->signals_report_enabled);
  EXPECT_TRUE(state->can_collect_all_fields);
  EXPECT_EQ(state->last_signals_upload_config, "test_config");
  EXPECT_FALSE(state->last_upload_attempt_timestamp.empty());
  EXPECT_FALSE(state->last_upload_success_timestamp.empty());
}

TEST_F(ConnectorsInternalsUtilsTest, ProcessReportGenerationResult_Error) {
  auto result = base::unexpected(
      enterprise_reporting::ReportGenerationError::kProfileEmptyReport);
  auto [error_info, signals_json] =
      ProcessReportGenerationResult(std::move(result));

  ASSERT_TRUE(error_info.has_value());
  EXPECT_EQ(*error_info,
            base::StringPrintf(
                "Report generation failed with error code: %d",
                static_cast<int>(enterprise_reporting::ReportGenerationError::
                                     kProfileEmptyReport)));
  EXPECT_FALSE(signals_json.has_value());
}

TEST_F(ConnectorsInternalsUtilsTest, ProcessReportGenerationResult_EmptyQueue) {
  enterprise_reporting::ReportRequestQueue queue;
  auto result = base::ok(std::move(queue));
  auto [error_info, signals_json] =
      ProcessReportGenerationResult(std::move(result));

  ASSERT_TRUE(error_info.has_value());
  EXPECT_EQ(*error_info, "Report generator returned an empty queue.");
  EXPECT_FALSE(signals_json.has_value());
}

#if BUILDFLAG(ENTERPRISE_PROXY)
TEST_F(ConnectorsInternalsUtilsTest, GetProvisioningDomainState_NullService) {
  auto state = GetProvisioningDomainState(nullptr);
  EXPECT_TRUE(state->pvd_configs.empty());
}

TEST_F(ConnectorsInternalsUtilsTest, GetProvisioningDomainState_Success) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;

  const char kDebugInfo[] = R"({
    "domains": [
      {
        "policy": {
          "pvd_id": "domain1.example.com",
          "endpoints": [
            {"url": "https://proxy1.example.com", "weight": 100}
          ]
        },
        "fetched_config": {
          "identifier": "domain1.example.com",
          "expires": "Wed, 21 Oct 2026 07:28:00 GMT",
          "routes": ["route1", "route2"]
        }
      }
    ]
  })";

  std::optional<base::DictValue> debug_info =
      base::JSONReader::ReadDict(kDebugInfo, base::JSON_PARSE_RFC);
  ASSERT_TRUE(debug_info.has_value());

  EXPECT_CALL(mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info).value()));

  auto state = GetProvisioningDomainState(&mock_service);

  ASSERT_EQ(state->pvd_configs.size(), 1u);
  EXPECT_EQ(state->pvd_configs[0]->pvd_id, "domain1.example.com");
  ASSERT_TRUE(state->pvd_configs[0]->expiration_time.has_value());
  base::Time expected_time;
  ASSERT_TRUE(
      base::Time::FromString("Wed, 21 Oct 2026 07:28:00 GMT", &expected_time));
  EXPECT_EQ(state->pvd_configs[0]->expiration_time.value(), expected_time);
}

TEST_F(ConnectorsInternalsUtilsTest, GetProvisioningDomainState_EdgeCases) {
  NiceMock<enterprise_net::MockEnterpriseProxyService> mock_service;
  base::DictValue debug_info;
  base::ListValue domains;

  // Non-dict entry (should be skipped)
  domains.Append("not a dict");

  // No policy.pvd_id, fallback to fetched_config.identifier
  base::DictValue domain2;
  domain2.Set("fetched_config", CreateFetchedConfig("fallback_id_2"));
  domains.Append(std::move(domain2));

  // Empty policy.pvd_id, fallback to fetched_config.identifier
  base::DictValue domain3;
  domain3.Set("policy", CreatePvdPolicy(""));
  domain3.Set("fetched_config", CreateFetchedConfig("fallback_id_3"));
  domains.Append(std::move(domain3));

  // Neither available
  base::DictValue domain4;
  domains.Append(std::move(domain4));

  // Both available, prefers policy.pvd_id
  base::DictValue domain5;
  domain5.Set("policy", CreatePvdPolicy("preferred_id"));
  domain5.Set("fetched_config", CreateFetchedConfig("ignored_id"));
  domains.Append(std::move(domain5));

  debug_info.Set("domains", std::move(domains));

  EXPECT_CALL(mock_service, GetDebugInfo())
      .WillOnce(Return(std::move(debug_info)));

  auto state = GetProvisioningDomainState(&mock_service);

  ASSERT_EQ(state->pvd_configs.size(), 4u);
  EXPECT_EQ(state->pvd_configs[0]->pvd_id, "fallback_id_2");
  EXPECT_EQ(state->pvd_configs[1]->pvd_id, "fallback_id_3");
  EXPECT_EQ(state->pvd_configs[2]->pvd_id, "");
  EXPECT_EQ(state->pvd_configs[3]->pvd_id, "preferred_id");
}
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

}  // namespace

}  // namespace enterprise_connectors::utils
