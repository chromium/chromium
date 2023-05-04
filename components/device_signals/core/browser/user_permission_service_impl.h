// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/device_signals/core/browser/user_permission_service.h"

class PrefService;

namespace policy {
class ManagementService;
}  // namespace policy

namespace device_signals {

class UserDelegate;

class UserPermissionServiceImpl : public UserPermissionService {
 public:
  UserPermissionServiceImpl(policy::ManagementService* management_service,
                            std::unique_ptr<UserDelegate> user_delegate,
                            PrefService* user_prefs);

  UserPermissionServiceImpl(const UserPermissionServiceImpl&) = delete;
  UserPermissionServiceImpl& operator=(const UserPermissionServiceImpl&) =
      delete;

  ~UserPermissionServiceImpl() override;

  // UserPermissionService:
  bool ShouldCollectConsent() override;
  UserPermission CanUserCollectSignals(
      const UserContext& user_context) override;
  UserPermission CanCollectSignals() override;

 private:
  // Returns whether the user has explicitly agreed to device signals being
  // shared or not.
  bool HasUserConsented() const;

  // Returns true if the device is Cloud-managed.
  bool IsDeviceCloudManaged() const;

  const raw_ptr<policy::ManagementService> management_service_;
  const std::unique_ptr<UserDelegate> user_delegate_;
  const raw_ptr<PrefService> user_prefs_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_
