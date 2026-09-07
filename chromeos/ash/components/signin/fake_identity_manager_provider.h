// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SIGNIN_FAKE_IDENTITY_MANAGER_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SIGNIN_FAKE_IDENTITY_MANAGER_PROVIDER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/signin/identity_manager_provider.h"
#include "components/account_id/account_id.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

// Test IdentityManagerProvider that maps account ids to IdentityManager
// instances. Installs itself as the process-wide provider on construction.
// Find() returns a manager only for accounts explicitly registered via
// SetIdentityManagerForAccount (and nullptr otherwise), so a manager is never
// handed back for a user that a test has not set up as signed-in.
class FakeIdentityManagerProvider : public IdentityManagerProvider {
 public:
  FakeIdentityManagerProvider();
  FakeIdentityManagerProvider(const FakeIdentityManagerProvider&) = delete;
  FakeIdentityManagerProvider& operator=(const FakeIdentityManagerProvider&) =
      delete;
  ~FakeIdentityManagerProvider() override;

  // Registers `identity_manager` for `account_id`. Passing nullptr clears the
  // association.
  void SetIdentityManagerForAccount(const AccountId& account_id,
                                    signin::IdentityManager* identity_manager);

  // IdentityManagerProvider:
  signin::IdentityManager* Find(const AccountId& account_id) override;

 private:
  std::map<AccountId, raw_ptr<signin::IdentityManager>> identity_managers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SIGNIN_FAKE_IDENTITY_MANAGER_PROVIDER_H_
