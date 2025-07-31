// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_IOS_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_IOS_H_

#include "base/enterprise_util.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

// This is used to determine if the device is managed.
// If "com.apple.configuration.managed" is found in the user defaults, the
// device is considered managed.
class POLICY_EXPORT DeviceManagementStatusProvider final
    : public ManagementStatusProvider {
 public:
  DeviceManagementStatusProvider();
  ~DeviceManagementStatusProvider() final;

  DeviceManagementStatusProvider(const DeviceManagementStatusProvider&) =
      delete;
  DeviceManagementStatusProvider& operator=(
      const DeviceManagementStatusProvider&) = delete;

 protected:
  // ManagementStatusProvider impl
  EnterpriseManagementAuthority FetchAuthority() final;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_IOS_H_
