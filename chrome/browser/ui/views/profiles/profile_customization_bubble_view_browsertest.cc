// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"

#include <string>

#include "base/optional.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class ProfileCustomizationBubbleBrowserTest : public DialogBrowserTest {
 public:
  ProfileCustomizationBubbleBrowserTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ProfileCustomizationBubbleView::CreateBubble(browser()->profile(),
                                                 GetAvatarButton());
  }

  // Returns the avatar button, which is the anchor view for the customization
  // bubble.
  views::View* GetAvatarButton() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* avatar_button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    DCHECK(avatar_button);
    return avatar_button;
  }
};

IN_PROC_BROWSER_TEST_F(ProfileCustomizationBubbleBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
