// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_service.h"

#include "base/sequenced_task_runner.h"

namespace policy {

ComponentCloudPolicyService::Delegate::~Delegate() {}

ComponentCloudPolicyService::ComponentCloudPolicyService(
    const std::string& policy_type,
    PolicySource policy_source,
    Delegate* delegate,
    SchemaRegistry* schema_registry,
    CloudPolicyCore* core,
    CloudPolicyClient* client,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : policy_installed_(true), weak_ptr_factory_(this) {}

ComponentCloudPolicyService::~ComponentCloudPolicyService() {}

// static
bool ComponentCloudPolicyService::SupportsDomain(PolicyDomain domain) {
  return false;
}

void ComponentCloudPolicyService::OnSchemaRegistryReady() {}

void ComponentCloudPolicyService::OnSchemaRegistryUpdated(
    bool has_new_schemas) {}

void ComponentCloudPolicyService::OnCoreConnected(CloudPolicyCore* core) {}

void ComponentCloudPolicyService::OnCoreDisconnecting(CloudPolicyCore* core) {}

void ComponentCloudPolicyService::OnRefreshSchedulerStarted(
    CloudPolicyCore* core) {}

void ComponentCloudPolicyService::OnStoreLoaded(CloudPolicyStore* store) {}

void ComponentCloudPolicyService::OnStoreError(CloudPolicyStore* store) {}

void ComponentCloudPolicyService::OnPolicyFetched(CloudPolicyClient* client) {}

void ComponentCloudPolicyService::OnRegistrationStateChanged(
    CloudPolicyClient* client) {}

void ComponentCloudPolicyService::OnClientError(CloudPolicyClient* client) {}

}  // namespace policy
