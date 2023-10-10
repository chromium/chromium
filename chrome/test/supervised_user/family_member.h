// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_FAMILY_MEMBER_H_
#define CHROME_TEST_SUPERVISED_USER_FAMILY_MEMBER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/ui/browser.h"
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

  // Urls of family member's blocklist and allowlist settings. Member must be a
  // supervised user who is a subject to parental controls.
  GURL GetBlockListUrlFor(FamilyMember& member) const;
  GURL GetAllowListUrlFor(FamilyMember& member) const;

  // Browsertest apis expect pointer.
  Browser* browser() const { return &browser_.get(); }

 private:
  signin::test::TestAccount account_;
  // Reference emphasizes that the browser always exists. The browser_ becomes
  // dangling here because it is managed in external lifecycle.
  raw_ref<Browser, DisableDanglingPtrDetection> browser_;
  signin::test::SignInFunctions sign_in_functions_;
};
}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_FAMILY_MEMBER_H_
