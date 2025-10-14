// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/android/android_os_signals_collector.h"

#include <array>
#include <utility>

#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ContainerEq;
using testing::Return;
using testing::StrictMock;

namespace {

using safe_browsing::HasHarmfulAppsResultStatus;
using safe_browsing::VerifyAppsEnabledResult;

constexpr char kFakeBrowserEnrollmentDomain[] = "fake.domain.google.com";
constexpr char kHarmfulAppsResultHistogramName[] =
    "Enterprise.DeviceSignals.HarmfulApps.Result";
constexpr char kHarmfulAppsCountHistogramName[] =
    "Enterprise.DeviceSignals.HarmfulApps.Count";

constexpr int kSampleHarmfulAppsErrorCode = 123;

}  // namespace

namespace device_signals {

// TODO(446918304): This class should be sharing a base class with
// `DesktopOsSignalsCollectorTest` to reduce code duplication.
class AndroidOsSignalsCollectorTest : public testing::Test {
 protected:
  void SetUp() override {
    auto mock_browser_cloud_policy_store =
        std::make_unique<policy::MockCloudPolicyStore>();
    mock_browser_cloud_policy_store_ = mock_browser_cloud_policy_store.get();
    mock_browser_cloud_policy_manager_ =
        std::make_unique<policy::MockCloudPolicyManager>(
            std::move(mock_browser_cloud_policy_store),
            task_environment_.GetMainThreadTaskRunner());
    signal_collector_ = std::make_unique<AndroidOsSignalsCollector>(
        mock_browser_cloud_policy_manager_.get());

    SetVerifyAppsResult(VerifyAppsEnabledResult::SUCCESS_NOT_ENABLED);
    SetHarmfulAppsResult(HasHarmfulAppsResultStatus::SUCCESS, 2);
  }

  void TearDown() override { mock_browser_cloud_policy_store_ = nullptr; }

  void SetFakeBrowserPolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_managed_by(kFakeBrowserEnrollmentDomain);
    mock_browser_cloud_policy_store_->set_policy_data_for_testing(
        std::move(policy_data));
  }

  // Helper function to check a subset of signals that should or should not be
  // collected based on permission.
  void CheckSignalsCollected(OsSignalsResponse& response,
                             bool can_collect_pii) {
    if (can_collect_pii) {
      EXPECT_EQ(response.display_name,
                base::android::device_info::device_name());
    } else {
      EXPECT_EQ(response.display_name, std::nullopt);
    }
    EXPECT_EQ(response.operating_system, policy::GetOSPlatform());
    EXPECT_EQ(response.os_version, base::SysInfo::OperatingSystemVersion());
    EXPECT_EQ(response.browser_version, version_info::GetVersionNumber());
    EXPECT_EQ(response.device_model, base::android::android_info::model());
    EXPECT_EQ(response.device_manufacturer,
              base::android::android_info::manufacturer());
    EXPECT_EQ(response.device_enrollment_domain, kFakeBrowserEnrollmentDomain);
    EXPECT_EQ(response.security_patch_ms,
              device_signals::GetSecurityPatchLevelEpoch());
    EXPECT_EQ(response.verified_apps_enabled, expected_verify_app_result_);
    EXPECT_EQ(response.has_potentially_harmful_apps,
              GetExpectedHarmfulAppsSignal());

    CheckUmaHistograms();
  }

  void CheckUmaHistograms() {
    histogram_tester_.ExpectUniqueSample(kHarmfulAppsResultHistogramName,
                                         expected_harmful_app_result_, 1);

    if (expected_harmful_app_result_ == HasHarmfulAppsResultStatus::SUCCESS) {
      histogram_tester_.ExpectUniqueSample(kHarmfulAppsCountHistogramName,
                                           expected_harmful_app_count_, 1);
    } else {
      histogram_tester_.ExpectTotalCount(kHarmfulAppsCountHistogramName, 0);
    }
  }

  void SetVerifyAppsResult(VerifyAppsEnabledResult result) {
    safe_browsing::SafeBrowsingApiHandlerBridge::GetInstance()
        .SetVerifyAppsEnableResultForTesting(result);
    expected_verify_app_result_ =
        (result == VerifyAppsEnabledResult::SUCCESS_ENABLED ||
         result == VerifyAppsEnabledResult::SUCCESS_ALREADY_ENABLED);
  }

  void SetHarmfulAppsResult(HasHarmfulAppsResultStatus result,
                            int num_of_apps) {
    safe_browsing::SafeBrowsingApiHandlerBridge::GetInstance()
        .SetHarmfulAppsResultForTesting(
            result, num_of_apps,
            result == HasHarmfulAppsResultStatus::SUCCESS
                ? 0
                : kSampleHarmfulAppsErrorCode);
    expected_harmful_app_result_ = result;
    expected_harmful_app_count_ = num_of_apps;
  }

  bool GetExpectedHarmfulAppsSignal() {
    return (expected_harmful_app_result_ ==
                HasHarmfulAppsResultStatus::SUCCESS &&
            expected_harmful_app_count_ != 0);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyManager>
      mock_browser_cloud_policy_manager_;
  raw_ptr<policy::MockCloudPolicyStore> mock_browser_cloud_policy_store_;
  std::unique_ptr<AndroidOsSignalsCollector> signal_collector_;
  bool expected_verify_app_result_;
  HasHarmfulAppsResultStatus expected_harmful_app_result_;
  int expected_harmful_app_count_;
  base::HistogramTester histogram_tester_;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
TEST_F(AndroidOsSignalsCollectorTest, SupportedOsSignalNames) {
  const std::array<SignalName, 1> supported_signals{{SignalName::kOsSignals}};

  const auto names_set = signal_collector_->GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Happy path test case for OS signals collection with full permission.
TEST_F(AndroidOsSignalsCollectorTest, GetSignal_Success) {
  SetFakeBrowserPolicyData();
  // Test when verify apps is enabled.
  SetVerifyAppsResult(VerifyAppsEnabledResult::SUCCESS_ENABLED);
  // Test when harmful apps detection fails.
  SetHarmfulAppsResult(HasHarmfulAppsResultStatus::LOCAL_FAILURE, 0);

  SignalName signal_name = SignalName::kOsSignals;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_->GetSignal(signal_name, UserPermission::kGranted,
                               empty_request, response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.os_signals_response);
  CheckSignalsCollected(response.os_signals_response.value(),
                        /*can_collect_pii=*/true);
}

// Tests that an unsupported signal is marked as unsupported.
TEST_F(AndroidOsSignalsCollectorTest, GetOsSignal_Unsupported) {
  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_->GetSignal(signal_name, UserPermission::kGranted,
                               empty_request, response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests that signal collection is still complete even when consent is missing.
TEST_F(AndroidOsSignalsCollectorTest, GetSignal_MissingConsent) {
  SetFakeBrowserPolicyData();

  SignalName signal_name = SignalName::kOsSignals;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_->GetSignal(signal_name, UserPermission::kMissingConsent,
                               empty_request, response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.os_signals_response);
  CheckSignalsCollected(response.os_signals_response.value(),
                        /*can_collect_pii=*/false);
}

// Tests that signal collection is halted if permission is not sufficient.
TEST_F(AndroidOsSignalsCollectorTest, GetSignal_MissingUser) {
  SignalName signal_name = SignalName::kOsSignals;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_->GetSignal(signal_name, UserPermission::kMissingUser,
                               empty_request, response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_FALSE(response.os_signals_response);
}

}  // namespace device_signals
