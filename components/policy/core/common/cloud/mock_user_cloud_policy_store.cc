// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"

#include "base/task/sequenced_task_runner.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

MockUserCloudPolicyStore::MockUserCloudPolicyStore()
    : UserCloudPolicyStore(base::FilePath(),
                           base::FilePath(),
                           scoped_refptr<base::SequencedTaskRunner>()) {}

MockUserCloudPolicyStore::~MockUserCloudPolicyStore() = default;

}  // namespace policy
