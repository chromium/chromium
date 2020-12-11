// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_POLICY_MANAGER_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_POLICY_MANAGER_IMPL_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/internal/identity_manager/primary_account_policy_manager.h"

class PrefService;
class PrimaryAccountManager;
class SigninClient;

class PrimaryAccountPolicyManagerImpl : public PrimaryAccountPolicyManager {
 public:
  explicit PrimaryAccountPolicyManagerImpl(SigninClient* client);
  ~PrimaryAccountPolicyManagerImpl() override;

  // PrimaryAccountPolicyManager:
  void InitializePolicy(
      PrefService* local_state,
      PrimaryAccountManager* primary_account_manager) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PrimaryAccountPolicyManagerImplTest, Prohibited);
  FRIEND_TEST_ALL_PREFIXES(PrimaryAccountPolicyManagerImplTest,
                           TestAlternateWildcard);

  // Returns true if a signin to Chrome is allowed (by policy or pref).
  bool IsSigninAllowed() const;

  void OnSigninAllowedPrefChanged(
      PrimaryAccountManager* primary_account_manager);
  void OnGoogleServicesUsernamePatternChanged(
      PrimaryAccountManager* primary_account_manager);

  // Returns true if the passed username is allowed by policy.
  bool IsAllowedUsername(const std::string& username) const;

  SigninClient* client_;

  // Helper object to listen for changes to signin preferences stored in non-
  // profile-specific local prefs (like kGoogleServicesUsernamePattern).
  PrefChangeRegistrar local_state_pref_registrar_;

  // Helper object to listen for changes to the signin allowed preference.
  BooleanPrefMember signin_allowed_;

  base::WeakPtrFactory<PrimaryAccountPolicyManagerImpl> weak_pointer_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PrimaryAccountPolicyManagerImpl);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PRIMARY_ACCOUNT_POLICY_MANAGER_IMPL_H_
