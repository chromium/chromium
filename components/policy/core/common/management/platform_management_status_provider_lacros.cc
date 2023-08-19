// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_status_provider_lacros.h"

#include "chromeos/startup/browser_params_proxy.h"

namespace policy {
DeviceEnterpriseManagedStatusProvider::DeviceEnterpriseManagedStatusProvider() =
    default;

EnterpriseManagementAuthority
DeviceEnterpriseManagedStatusProvider::FetchAuthority() {
  return chromeos::BrowserParamsProxy::Get()->IsDeviceEnterprisedManaged()
             ? CLOUD_DOMAIN
             : NONE;
}

}  // namespace policy
