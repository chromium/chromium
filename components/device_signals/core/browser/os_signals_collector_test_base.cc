// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/os_signals_collector_test_base.h"

#include "base/check.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"

namespace device_signals {

GenericOsSignalsCollectorTestBase::GenericOsSignalsCollectorTestBase() =
    default;

GenericOsSignalsCollectorTestBase::~GenericOsSignalsCollectorTestBase() =
    default;

void GenericOsSignalsCollectorTestBase::SetUp() {
  CHECK(task_environment_);
  auto mock_browser_cloud_policy_store =
      std::make_unique<policy::MockCloudPolicyStore>(
          policy::dm_protocol::GetChromeUserPolicyType());
  mock_browser_cloud_policy_store_ = mock_browser_cloud_policy_store.get();
  mock_browser_cloud_policy_manager_ =
      std::make_unique<policy::MockCloudPolicyManager>(
          std::move(mock_browser_cloud_policy_store),
          task_environment_->GetMainThreadTaskRunner());
}

void GenericOsSignalsCollectorTestBase::TearDown() {
  mock_browser_cloud_policy_store_ = nullptr;
}

void GenericOsSignalsCollectorTestBase::SetFakeBrowserPolicyData() {
  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->set_managed_by(kFakeBrowserEnrollmentDomain);
  mock_browser_cloud_policy_store_->set_policy_data_for_testing(
      std::move(policy_data));
}

void GenericOsSignalsCollectorTestBase::CheckSignalsCollected(
    OsSignalsResponse& response,
    bool can_collect_pii,
    const std::string& device_name) {
  EXPECT_EQ(response.device_enrollment_domain, kFakeBrowserEnrollmentDomain);
  EXPECT_EQ(response.browser_version, version_info::GetVersionNumber());
  EXPECT_EQ(response.operating_system, policy::GetOSPlatform());

  if (can_collect_pii) {
    EXPECT_EQ(response.display_name, device_name);
  } else {
    EXPECT_EQ(response.display_name, std::nullopt);
  }
}

}  // namespace device_signals
