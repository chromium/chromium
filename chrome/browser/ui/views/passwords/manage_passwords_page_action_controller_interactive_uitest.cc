// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_page_action_controller.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

class ManagePasswordsControllerTest : public ManagePasswordsTest {
 public:
  ManagePasswordsControllerTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageActionsMigration,
        {{features::kPageActionsMigrationManagePasswords.name, "true"}});
  }

  ~ManagePasswordsControllerTest() override = default;

  password_manager::ui::State GetViewState() {
    return GetController()->GetState();
  }

  views::View* GetIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionView(kActionShowPasswordsBubbleOrPage);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsControllerTest,
                       IconIsVisibleInManageState) {
  // Make sure the icon is not showing initially.
  ASSERT_TRUE(GetIcon());
  EXPECT_FALSE(GetIcon()->GetVisible());
  SetupManagingPasswords();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, GetViewState());
  // The icon should show in the new state.
  ASSERT_TRUE(GetIcon());
  EXPECT_TRUE(GetIcon()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsControllerTest,
                       IconIsHiddenInDefaultInactiveState) {
  SetupManagingPasswords();
  // Make sure the icon is showing initially.
  ASSERT_TRUE(GetIcon());
  EXPECT_TRUE(GetIcon()->GetVisible());
  // Navigate to a new blank page. This action causes the
  // ManagePasswordsUIController for the tab to reset its state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  // The icon should not show in the new state.
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, GetViewState());
  ASSERT_TRUE(GetIcon());
  EXPECT_FALSE(GetIcon()->GetVisible());
}
