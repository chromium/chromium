// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

class COMPONENT_EXPORT(IDENTITY_MANAGER_PROVIDER) IdentityManagerProvider {
 public:
  IdentityManagerProvider();
  IdentityManagerProvider(const IdentityManagerProvider&) = delete;
  IdentityManagerProvider& operator=(const IdentityManagerProvider&) = delete;
  virtual ~IdentityManagerProvider();

  // Returns the process-wide singleton.
  static IdentityManagerProvider& Get();

  // Returns the IdentityManager for the User corresponding to `account_id`.
  // The caller must ensure the User's BrowserContext (profile) is alive.
  // Returns nullptr only if IdentityManager is not available for that
  // profile, such as for Guest profiles.
  virtual signin::IdentityManager* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_
