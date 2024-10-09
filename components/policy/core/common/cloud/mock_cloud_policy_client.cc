// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

MockCloudPolicyClient::MockCloudPolicyClient()
    : MockCloudPolicyClient(nullptr, nullptr) {}

MockCloudPolicyClient::MockCloudPolicyClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : MockCloudPolicyClient(url_loader_factory, nullptr) {}

MockCloudPolicyClient::MockCloudPolicyClient(DeviceManagementService* service)
    : MockCloudPolicyClient(nullptr, service) {}

MockCloudPolicyClient::MockCloudPolicyClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DeviceManagementService* service)
    : CloudPolicyClient(service,
                        std::move(url_loader_factory),
                        CloudPolicyClient::DeviceDMTokenCallback()) {}

MockCloudPolicyClient::~MockCloudPolicyClient() = default;

void MockCloudPolicyClient::SetDMToken(const std::string& token) {
  dm_token_ = token;
}

void MockCloudPolicyClient::SetPolicy(const std::string& policy_type,
                                      const std::string& settings_entity_id,
                                      const em::PolicyFetchResponse& policy) {
  last_policy_fetch_responses_[std::make_pair(policy_type,
                                              settings_entity_id)] = policy;
}

void MockCloudPolicyClient::SetFetchedInvalidationVersion(
    int64_t fetched_invalidation_version) {
  fetched_invalidation_version_ = fetched_invalidation_version;
}

void MockCloudPolicyClient::SetStatus(DeviceManagementStatus status) {
  last_dm_status_ = status;
}

MockCloudPolicyClientObserver::MockCloudPolicyClientObserver() = default;

MockCloudPolicyClientObserver::~MockCloudPolicyClientObserver() = default;

}  // namespace policy
