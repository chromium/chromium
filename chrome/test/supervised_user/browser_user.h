// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_BROWSER_USER_H_
#define CHROME_TEST_SUPERVISED_USER_BROWSER_USER_H_

#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/supervised_user/test_support/account_repository.h"
#include "components/supervised_user/test_support/family_link_settings_state_management.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace supervised_user {

// Represents the user of the browser, who is typically a family member: a
// parent or a child.
class BrowserUser {
 public:
  using NewTabCallback =
      base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>;

  BrowserUser(test_accounts::FamilyMember credentials,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              signin::IdentityManager& identity_manager,
              Browser& browser,
              Profile& profile,
              const NewTabCallback add_tab_function);
  ~BrowserUser();

  void TurnOnSync();
  void SignOutFromWeb();
  void SignInFromWeb();

  CoreAccountId GetAccountId() const;
  std::string_view GetAccountPassword() const;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() const {
    return url_loader_factory_;
  }
  signin::IdentityManager& identity_manager() const {
    return identity_manager_.get();
  }

  // These services can be used to verify browser state.
  FamilyLinkSettingsState::Services GetServices() const;

  Browser& browser() const { return browser_.get(); }
  Profile& profile() const { return profile_.get(); }

 private:
  const test_accounts::FamilyMember credentials_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ref<signin::IdentityManager> identity_manager_;
  raw_ref<Browser> browser_;
  raw_ref<Profile> profile_;

  signin::test::SignInFunctions sign_in_functions_;
};
}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_BROWSER_USER_H_
