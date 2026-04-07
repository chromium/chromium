// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/desktop/desktop_os_signals_collector.h"

#include <array>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/device_signals/core/browser/os_signals_collector_test_base.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ContainerEq;
using testing::Return;
using testing::StrictMock;

namespace device_signals {

class DesktopOsSignalsCollectorTest : public GenericOsSignalsCollectorTestBase {
 protected:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>();
    GenericOsSignalsCollectorTestBase::SetUp();
    signal_collector_ = std::make_unique<DesktopOsSignalsCollector>(
        mock_browser_cloud_policy_manager_.get());
  }

  std::unique_ptr<DesktopOsSignalsCollector> signal_collector_;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
TEST_F(DesktopOsSignalsCollectorTest, SupportedOsSignalNames) {
  const std::array<SignalName, 1> supported_signals{{SignalName::kOsSignals}};

  const auto names_set = signal_collector_->GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Happy path test case for OS signals collection with full permission.
TEST_F(DesktopOsSignalsCollectorTest, GetSignal_Success) {
  SetFakeBrowserPolicyData();

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
                        /*can_collect_pii=*/true, policy::GetDeviceName());
}

// Tests that an unsupported signal is marked as unsupported.
TEST_F(DesktopOsSignalsCollectorTest, GetOsSignal_Unsupported) {
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
TEST_F(DesktopOsSignalsCollectorTest, GetSignal_MissingConsent) {
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
                        /*can_collect_pii=*/false, policy::GetDeviceName());
}

// Tests that signal collection is halted if permission is not sufficient.
TEST_F(DesktopOsSignalsCollectorTest, GetSignal_MissingUser) {
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
