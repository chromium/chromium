// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_MANAGER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace policy {

class CloudPolicyStore;

class MockCloudPolicyManager : public CloudPolicyManager {
 public:
  MockCloudPolicyManager(
      std::unique_ptr<CloudPolicyStore> store,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  MockCloudPolicyManager(const MockCloudPolicyManager&) = delete;
  MockCloudPolicyManager& operator=(const MockCloudPolicyManager&) = delete;

  ~MockCloudPolicyManager() override;

  // Publish the protected members for testing.
  using CloudPolicyManager::CheckAndPublishPolicy;
  using CloudPolicyManager::client;
  using CloudPolicyManager::service;
  using CloudPolicyManager::store;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_MANAGER_H_
