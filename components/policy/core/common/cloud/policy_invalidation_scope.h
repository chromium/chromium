// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_SCOPE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_SCOPE_H_

namespace policy {

// Specifies a scope of a policy or remote command which handler is
// responsible for.
enum class PolicyInvalidationScope {
  kUser,
  kDevice,
  kDeviceLocalAccount,
  kCBCM,
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_POLICY_INVALIDATION_SCOPE_H_
