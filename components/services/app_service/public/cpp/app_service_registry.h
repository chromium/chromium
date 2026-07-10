// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_REGISTRY_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_REGISTRY_H_

#include <map>

#include "base/component_export.h"
#include "components/account_id/account_id.h"

static_assert(BUILDFLAG(IS_CHROMEOS));

namespace apps {

class AppService;

// Maintains the instances of AppService for each User.
// This class does NOT own the AppService instance.
class COMPONENT_EXPORT(APP_SERVICE) AppServiceRegistry {
 public:
  AppServiceRegistry();
  AppServiceRegistry(const AppServiceRegistry&) = delete;
  AppServiceRegistry& operator=(const AppServiceRegistry&) = delete;
  ~AppServiceRegistry();

  // Returns the global singleton instance of this manager.
  static AppServiceRegistry* Get();

  // Returns the AppService instance for the user represented by the given
  // `account_id`. If there is no instance created, nullptr is returned.
  AppService* Find(const AccountId& account_id);

  // Registers the given `app_service` as the User's AppService.
  // Unregister needs to be called later, and `app_service` must outlive
  // until the call.
  void Register(const AccountId& account_id, AppService* app_service);

  // Unregisters the AppService instance for the User represted by
  // the `account_id`.
  void Unregister(const AccountId& account_id);

 private:
  std::map<AccountId, AppService*> service_map_;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_SERVICE_REGISTRY_H_
