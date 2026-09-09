// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_internals_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/device_trust/core/common_types.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID)

namespace enterprise_connectors::utils {

TEST(ConnectorsInternalsUtilsTest, GetStringFromTimestamp) {
  EXPECT_EQ(GetStringFromTimestamp(base::Time()), std::string());

  base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  EXPECT_FALSE(GetStringFromTimestamp(time).empty());
}

#if !BUILDFLAG(IS_ANDROID)
TEST(ConnectorsInternalsUtilsTest, ConvertPolicyLevelToString) {
  EXPECT_EQ(ConvertPolicyLevelToString(DTCPolicyLevel::kBrowser),
            kBrowserLevel);
  EXPECT_EQ(ConvertPolicyLevelToString(DTCPolicyLevel::kUser), kUserLevel);
}

TEST(ConnectorsInternalsUtilsTest, GetPolicyEnabledLevels_NullService) {
  EXPECT_TRUE(GetPolicyEnabledLevels(nullptr).empty());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST(ConnectorsInternalsUtilsTest, CreateUnsupportedDeviceTrustState) {
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

TEST(ConnectorsInternalsUtilsTest, CreateDeviceTrustStateWithNoKey) {
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

TEST(ConnectorsInternalsUtilsTest, CreateSignalsReportingState_WithPrefs) {
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

TEST(ConnectorsInternalsUtilsTest, ProcessReportGenerationResult_Error) {
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

TEST(ConnectorsInternalsUtilsTest, ProcessReportGenerationResult_EmptyQueue) {
  enterprise_reporting::ReportRequestQueue queue;
  auto result = base::ok(std::move(queue));
  auto [error_info, signals_json] =
      ProcessReportGenerationResult(std::move(result));

  ASSERT_TRUE(error_info.has_value());
  EXPECT_EQ(*error_info, "Report generator returned an empty queue.");
  EXPECT_FALSE(signals_json.has_value());
}

}  // namespace enterprise_connectors::utils
