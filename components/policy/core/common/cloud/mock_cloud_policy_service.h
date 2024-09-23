// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_SERVICE_H_

#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class CloudPolicyClient;
class CloudPolicyStore;

class MockCloudPolicyService : public CloudPolicyService {
 public:
  MockCloudPolicyService(CloudPolicyClient* client, CloudPolicyStore* store);
  MockCloudPolicyService(const MockCloudPolicyService&) = delete;
  MockCloudPolicyService& operator=(const MockCloudPolicyService&) = delete;
  ~MockCloudPolicyService() override;

  MOCK_METHOD2(RefreshPolicy, void(RefreshPolicyCallback, PolicyFetchReason));

 private:
  // Invokes real RefreshPolicy() method.
  void InvokeRefreshPolicy(RefreshPolicyCallback callback,
                           PolicyFetchReason reason);
};

class MockCloudPolicyServiceObserver : public CloudPolicyService::Observer {
 public:
  MockCloudPolicyServiceObserver();
  MockCloudPolicyServiceObserver(const MockCloudPolicyServiceObserver&) =
      delete;
  MockCloudPolicyServiceObserver& operator=(
      const MockCloudPolicyServiceObserver&) = delete;
  ~MockCloudPolicyServiceObserver() override;

  MOCK_METHOD0(OnCloudPolicyServiceInitializationCompleted, void());

  std::string_view name() const override;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_MOCK_CLOUD_POLICY_SERVICE_H_
