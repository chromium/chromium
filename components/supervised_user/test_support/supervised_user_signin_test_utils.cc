// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"

#include "base/check.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace supervised_user {

void UpdateSupervisionStatusForAccount(
    AccountInfo& account,
    signin::IdentityManager* identity_manager,
    bool is_subject_to_parental_controls) {
  CHECK(identity_manager);
  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(is_subject_to_parental_controls);
  // Update child status preference, which is backed by capability state.
  // This action will not be performed by the fake account capability fetcher.
  account.is_child_account = is_subject_to_parental_controls
                                 ? signin::Tribool::kTrue
                                 : signin::Tribool::kFalse;
  signin::UpdateAccountInfoForAccount(identity_manager, account);
}

}  // namespace supervised_user
