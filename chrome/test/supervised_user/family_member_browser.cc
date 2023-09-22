// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/family_member_browser.h"

#include "base/test/bind.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/browser/ui/browser.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace supervised_user {

FamilyMemberBrowser::FamilyMemberBrowser(
    signin::test::TestAccount account,
    Browser& browser,
    const base::RepeatingCallback<bool(int, const GURL&, ui::PageTransition)>
        add_tab_function)
    : account_(account),
      browser_(browser),
      sign_in_functions_(base::BindLambdaForTesting(
                             [&browser]() -> Browser* { return &browser; }),
                         add_tab_function) {}
}  // namespace supervised_user
