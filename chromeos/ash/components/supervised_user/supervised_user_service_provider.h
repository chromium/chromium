// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SUPERVISED_USER_SUPERVISED_USER_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SUPERVISED_USER_SUPERVISED_USER_SERVICE_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace supervised_user {
class SupervisedUserService;
}  // namespace supervised_user

namespace ash {

// Provides the supervised_user::SupervisedUserService associated with a user to
// ChromeOS callers without forcing them to depend on
// //chrome/browser/supervised_user's Profile-keyed factory. The concrete
// implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// supervised_user_service_provider_impl.h).
class COMPONENT_EXPORT(SUPERVISED_USER_SERVICE_PROVIDER)
    SupervisedUserServiceProvider {
 public:
  SupervisedUserServiceProvider();
  SupervisedUserServiceProvider(const SupervisedUserServiceProvider&) = delete;
  SupervisedUserServiceProvider& operator=(
      const SupervisedUserServiceProvider&) = delete;
  virtual ~SupervisedUserServiceProvider();

  // Returns the process-wide singleton.
  static SupervisedUserServiceProvider& Get();

  // Returns the SupervisedUserService associated with `account_id`, or nullptr
  // if none is available. The returned pointer is owned by the
  // BrowserContext-keyed service infrastructure; callers must not delete it.
  virtual supervised_user::SupervisedUserService* Find(
      const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SUPERVISED_USER_SUPERVISED_USER_SERVICE_PROVIDER_H_
