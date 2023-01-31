// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/device_signals/core/browser/user_permission_service.h"

namespace policy {
class ManagementService;
}  // namespace policy

namespace device_signals {

class UserDelegate;

class UserPermissionServiceImpl : public UserPermissionService {
 public:
  UserPermissionServiceImpl(policy::ManagementService* management_service,
                            std::unique_ptr<UserDelegate> user_delegate);

  UserPermissionServiceImpl(const UserPermissionServiceImpl&) = delete;
  UserPermissionServiceImpl& operator=(const UserPermissionServiceImpl&) =
      delete;

  ~UserPermissionServiceImpl() override;

  // UserPermissionService:
  void CanUserCollectSignals(const UserContext& user_context,
                             CanCollectCallback callback) override;
  void CanCollectSignals(CanCollectCallback callback) override;

 private:
  base::raw_ptr<policy::ManagementService> management_service_;
  std::unique_ptr<UserDelegate> user_delegate_;

  base::WeakPtrFactory<UserPermissionServiceImpl> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_USER_PERMISSION_SERVICE_IMPL_H_
