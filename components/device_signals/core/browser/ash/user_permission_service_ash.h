// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ASH_USER_PERMISSION_SERVICE_ASH_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ASH_USER_PERMISSION_SERVICE_ASH_H_

#include <memory>

#include "components/device_signals/core/browser/user_permission_service_impl.h"

class PrefService;

namespace policy {
class ManagementService;
}  // namespace policy

namespace device_signals {

class UserDelegate;

class UserPermissionServiceAsh : public UserPermissionServiceImpl {
 public:
  UserPermissionServiceAsh(policy::ManagementService* management_service,
                           std::unique_ptr<UserDelegate> user_delegate,
                           PrefService* user_prefs);

  UserPermissionServiceAsh(const UserPermissionServiceAsh&) = delete;
  UserPermissionServiceAsh& operator=(const UserPermissionServiceAsh&) = delete;

  ~UserPermissionServiceAsh() override;

  // UserPermissionService:
  bool ShouldCollectConsent() const override;
  UserPermission CanCollectSignals() const override;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ASH_USER_PERMISSION_SERVICE_ASH_H_
