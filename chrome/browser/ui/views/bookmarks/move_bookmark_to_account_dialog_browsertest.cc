// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/move_bookmark_to_account_dialog.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

class MoveBookmarksToAccountDialogTest : public DialogBrowserTest {
 public:
  MoveBookmarksToAccountDialogTest() = default;

  MoveBookmarksToAccountDialogTest(const MoveBookmarksToAccountDialogTest&) =
      delete;
  MoveBookmarksToAccountDialogTest& operator=(
      const MoveBookmarksToAccountDialogTest&) = delete;

  ~MoveBookmarksToAccountDialogTest() override = default;

  void ShowUi(const std::string& name) override {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "foo@gmail.com", signin::ConsentLevel::kSignin);
    signin::SimulateAccountImageFetch(identity_manager, account_info.account_id,
                                      "https://avatar.com/avatar.png",
                                      gfx::test::CreateImage(/*size=*/32));

    ShowMoveBookmarkToAccountDialog(browser());
  }
};

IN_PROC_BROWSER_TEST_F(MoveBookmarksToAccountDialogTest, Show) {
  set_baseline("5807574");

  ShowAndVerifyUi();
}

}  // namespace
