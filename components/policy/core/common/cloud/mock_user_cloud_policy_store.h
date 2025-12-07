// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_USER_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_USER_CLOUD_POLICY_STORE_H_

#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockUserCloudPolicyStore : public UserCloudPolicyStore {
 public:
  MockUserCloudPolicyStore();
  MockUserCloudPolicyStore(const MockUserCloudPolicyStore&) = delete;
  MockUserCloudPolicyStore& operator=(const MockUserCloudPolicyStore&) = delete;
  ~MockUserCloudPolicyStore() override;

  MOCK_METHOD1(Store, void(const enterprise_management::PolicyFetchResponse&));
  MOCK_METHOD0(Load, void(void));
  MOCK_METHOD0(LoadImmediately, void(void));
  MOCK_METHOD0(Clear, void(void));
  MOCK_METHOD0(ResetPolicyKey, void(void));

  // Publish the protected members.
  using CloudPolicyStore::NotifyStoreLoaded;
  using CloudPolicyStore::NotifyStoreError;

  using CloudPolicyStore::policy_map_;
  using CloudPolicyStore::status_;
  using CloudPolicyStore::validation_result_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_USER_CLOUD_POLICY_STORE_H_
