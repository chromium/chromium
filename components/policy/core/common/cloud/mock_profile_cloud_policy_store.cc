// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_profile_cloud_policy_store.h"

#include "base/task/sequenced_task_runner.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

MockProfileCloudPolicyStore::MockProfileCloudPolicyStore()
    : ProfileCloudPolicyStore(base::FilePath(),
                              base::FilePath(),
                              scoped_refptr<base::SequencedTaskRunner>()) {}

MockProfileCloudPolicyStore::~MockProfileCloudPolicyStore() = default;

}  // namespace policy
