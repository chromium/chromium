// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

class ManagePasswordsUIControllerBrowserTest : public ManagePasswordsTest {};

// Regression test for crbug.com/485738514.
// Verifies that a background tab correctly updates its own PageActionController
// and doesn't incorrectly target the active tab's interface.
IN_PROC_BROWSER_TEST_F(ManagePasswordsUIControllerBrowserTest,
                       MigratedPageActionUpdatesCorrectTab) {
  // 1. Setup Background Tab (Tab 0)
  content::WebContents* background_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ManagePasswordsUIController* background_controller =
      ManagePasswordsUIController::FromWebContents(background_contents);
  ASSERT_TRUE(background_controller);

  // 2. Setup Foreground Tab (Tab 1)
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  content::WebContents* foreground_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(background_contents, foreground_contents);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // 3. Trigger Update on Background Controller
  std::vector<password_manager::PasswordForm> forms;
  forms.emplace_back();
  forms[0].url = GURL("http://example.com");
  forms[0].signon_realm = "http://example.com/";
  forms[0].username_value = u"user";
  forms[0].password_value = u"pass";

  // Triggering OnPasswordAutofilled will call UpdateBubbleAndIconVisibility.
  // In the buggy version, this would use browser->GetActiveTabInterface()
  // and thus update the PageActionController of the foreground tab.
  background_controller->OnPasswordAutofilled(
      forms, url::Origin::Create(forms[0].url), {});

  // 4. Verify Foreground Tab Icon Visibility
  // The foreground tab's page action icon should NOT be visible.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* icon_view =
      browser_view->toolbar_button_provider()->GetPageActionView(
          kActionShowPasswordsBubbleOrPage);
  ASSERT_TRUE(icon_view);
  EXPECT_FALSE(icon_view->GetVisible())
      << "Foreground PageActionView was incorrectly shown by background tab "
         "update.";
}
