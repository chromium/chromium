// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_PROTO_TYPES_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_PROTO_TYPES_H_

#include <set>

#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

struct POLICY_EXPORT ExtensionInstallDecision {
  ExtensionInstallDecision();
  ExtensionInstallDecision(
      enterprise_management::ExtensionInstallPolicy::Action action,
      std::set<enterprise_management::ExtensionInstallPolicy::Reason> reasons);
  ExtensionInstallDecision(const ExtensionInstallDecision&);
  ExtensionInstallDecision(ExtensionInstallDecision&&);
  ~ExtensionInstallDecision();

  enterprise_management::ExtensionInstallPolicy::Action action =
      enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW;
  std::set<enterprise_management::ExtensionInstallPolicy::Reason> reasons;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_PROTO_TYPES_H_
