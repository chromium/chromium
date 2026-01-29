// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"

#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

MockCloudPolicyStore::MockCloudPolicyStore(const std::string& policy_type)
    : CloudPolicyStore(policy_type) {}

MockCloudPolicyStore::~MockCloudPolicyStore() = default;

MockCloudPolicyStoreObserver::MockCloudPolicyStoreObserver() = default;

MockCloudPolicyStoreObserver::~MockCloudPolicyStoreObserver() = default;

}  // namespace policy
