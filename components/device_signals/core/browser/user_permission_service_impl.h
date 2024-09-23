// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

  // Returns a WeakPtr for the current service.
  base::WeakPtr<UserPermissionServiceImpl> GetWeakPtr();

  // UserPermissionService:
  bool ShouldCollectConsent() const override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  UserPermission CanUserCollectSignals(
      const UserContext& user_context) const override;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX
  UserPermission CanCollectSignals() const override;
  bool HasUserConsented() const override;

 protected:
  // Returns true if the specific consent flow policy is enabled.
  bool IsConsentFlowPolicyEnabled() const;

  // Returns true if the device is Cloud-managed.
  bool IsDeviceCloudManaged() const;

  const raw_ptr<policy::ManagementService> management_service_;
  const std::unique_ptr<UserDelegate> user_delegate_;
  const raw_ptr<PrefService> user_prefs_;

  base::WeakPtrFactory<UserPermissionServiceImpl> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_
