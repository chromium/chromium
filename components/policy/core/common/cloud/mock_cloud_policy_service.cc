// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace policy {

MockCloudPolicyService::MockCloudPolicyService(CloudPolicyClient* client,
                                               CloudPolicyStore* store)
    : CloudPolicyService(std::string(), std::string(), client, store) {
  // Besides recording the mock call, invoke real RefreshPolicy() method.
  // That way FetchPolicy() is called on the |client|.
  ON_CALL(*this, RefreshPolicy(testing::_, testing::_))
      .WillByDefault(
          testing::Invoke(this, &MockCloudPolicyService::InvokeRefreshPolicy));
}

MockCloudPolicyService::~MockCloudPolicyService() = default;

void MockCloudPolicyService::InvokeRefreshPolicy(RefreshPolicyCallback callback,
                                                 PolicyFetchReason reason) {
  CloudPolicyService::RefreshPolicy(std::move(callback), reason);
}

MockCloudPolicyServiceObserver::MockCloudPolicyServiceObserver() = default;

MockCloudPolicyServiceObserver::~MockCloudPolicyServiceObserver() = default;

std::string_view MockCloudPolicyServiceObserver::name() const {
  return "MockCloudPolicyServiceObserver";
}

}  // namespace policy
