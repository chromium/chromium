// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_cloud_policy_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "services/network/test/test_network_connection_tracker.h"

namespace policy {

MockCloudPolicyManager::MockCloudPolicyManager(
    std::unique_ptr<CloudPolicyStore> store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : CloudPolicyManager(
          dm_protocol::kChromeUserPolicyType,
          std::string(),
          std::move(store),
          task_runner,
          network::TestNetworkConnectionTracker::CreateGetter()) {}

MockCloudPolicyManager::~MockCloudPolicyManager() = default;

}  // namespace policy
