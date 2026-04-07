// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_OS_SIGNALS_COLLECTOR_TEST_BASE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_OS_SIGNALS_COLLECTOR_TEST_BASE_H_

#include <array>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
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

namespace device_signals {

inline constexpr char kFakeBrowserEnrollmentDomain[] = "fake.domain.google.com";

class GenericOsSignalsCollectorTestBase : public testing::Test {
 public:
  GenericOsSignalsCollectorTestBase();
  ~GenericOsSignalsCollectorTestBase() override;

 protected:
  void SetUp() override;
  void SetFakeBrowserPolicyData();
  void TearDown() override;

  // Helper function to check a subset of signals that should or should not be
  // collected based on permission. Not all signals are checked due to testing
  // limitation.
  void CheckSignalsCollected(OsSignalsResponse& response,
                             bool can_collect_pii,
                             const std::string& device_name);

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<policy::MockCloudPolicyManager>
      mock_browser_cloud_policy_manager_;
  raw_ptr<policy::MockCloudPolicyStore> mock_browser_cloud_policy_store_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_OS_SIGNALS_COLLECTOR_TEST_BASE_H_
