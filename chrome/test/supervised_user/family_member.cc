// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_member.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace supervised_user {

FamilyMember::FamilyMember(
    signin::test::TestAccount account,
    Browser& browser,
    const base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>
        add_tab_function)
    : account_(account),
      browser_(browser),
      sign_in_functions_(base::BindLambdaForTesting(
                             [&browser]() -> Browser* { return &browser; }),
                         add_tab_function) {}
FamilyMember::~FamilyMember() = default;

GURL FamilyMember::GetBlockListUrlFor(FamilyMember& member) const {
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(member.browser_->profile());
  CHECK(supervised_user_service->IsURLFilteringEnabled())
      << "Blocklist control page is only available to user who have that "
         "feature enabled. Check if member is a subject to parental controls.";

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(member.browser_->profile());

  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  return GURL(
      base::StrCat({"https://families.google.com/u/0/manage/family/child/",
                    account_id.ToString(), "/exceptions/blocked"}));
}

void FamilyMember::TurnOnSync() {
  sign_in_functions_.TurnOnSync(account_, 0);
}
}  // namespace supervised_user
