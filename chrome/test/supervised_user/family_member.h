// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_FAMILY_MEMBER_H_
#define CHROME_TEST_SUPERVISED_USER_FAMILY_MEMBER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace supervised_user {

// Family member's actions that can be taken in browser UI.
class FamilyMember {
 public:
  using NewTabCallback =
      base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>;

  FamilyMember(signin::test::TestAccount account_,
               Browser& browser,
               const NewTabCallback add_tab_function);
  ~FamilyMember();

  void TurnOnSync();
  void SignOutFromWeb();

  // Urls of family member's blocklist and allowlist settings. Member must be a
  // supervised user who is a subject to parental controls.
  GURL GetBlockListUrlFor(const FamilyMember& member) const;
  GURL GetAllowListUrlFor(const FamilyMember& member) const;
  GURL GetPermissionsUrlFor(const FamilyMember& member) const;

  // Browsertest apis expect pointer.
  Browser* browser() const { return &browser_.get(); }

  signin::IdentityManager* identity_manager() const {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  SupervisedUserService* supervised_user_service() const;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() const {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetURLLoaderFactoryForBrowserProcess();
  }

  CoreAccountId GetAccountId() const;

  std::string_view GetAccountPassword() const;

 private:
  signin::test::TestAccount account_;
  // Reference emphasizes that the browser always exists. The browser_ becomes
  // dangling here because it is managed in external lifecycle.
  raw_ref<Browser, DisableDanglingPtrDetection> browser_;
  signin::test::SignInFunctions sign_in_functions_;
};
}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_FAMILY_MEMBER_H_
