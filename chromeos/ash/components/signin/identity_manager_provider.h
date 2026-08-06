// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_

class AccountId;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

class IdentityManagerProvider {
 public:
  IdentityManagerProvider();
  IdentityManagerProvider(const IdentityManagerProvider&) = delete;
  IdentityManagerProvider& operator=(const IdentityManagerProvider&) = delete;
  virtual ~IdentityManagerProvider();

  // Returns the process-wide singleton.
  static IdentityManagerProvider& Get();

  // Returns IdentityManager instance for the User corresponding to the
  // `account_id`. Or, nullptr if it is not available.
  virtual signin::IdentityManager* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SIGNIN_IDENTITY_MANAGER_PROVIDER_H_
