// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "ui/views/window/dialog_client_view.h"

class BookmarkBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  BookmarkBubbleViewBrowserTest() {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
#if !defined(OS_CHROMEOS)
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    if (name == "bookmark_details") {
      signin::ClearPrimaryAccount(identity_manager,
                                  signin::ClearPrimaryAccountPolicy::DEFAULT);
    } else {
      constexpr char kTestUserEmail[] = "testuser@gtest.com";
      signin::MakePrimaryAccountAvailable(identity_manager, kTestUserEmail);
    }
#endif

    const GURL url = GURL("https://www.google.com");
    const base::string16 title = base::ASCIIToUTF16("Title");
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
    bookmarks::AddIfNotBookmarked(bookmark_model, url, title);
    browser()->window()->ShowBookmarkBubble(url, true);

    if (name == "ios_promotion") {
      BookmarkBubbleView::bookmark_bubble()
          ->GetWidget()
          ->client_view()
          ->AsDialogClientView()
          ->AcceptWindow();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleViewBrowserTest);
};

// ChromeOS is always signed in.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(BookmarkBubbleViewBrowserTest,
                       InvokeUi_bookmark_details_signed_in) {
  ShowAndVerifyUi();
}
