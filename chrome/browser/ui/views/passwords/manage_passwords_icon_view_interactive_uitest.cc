// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

class ManagePasswordsIconViewTest : public ManagePasswordsTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  ManagePasswordsIconViewTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageActionsMigration,
        {{features::kPageActionsMigrationManagePasswords.name,
          IsMigrationEnabled() ? "true" : "false"}});
  }

  ManagePasswordsIconViewTest(const ManagePasswordsIconViewTest&) = delete;
  ManagePasswordsIconViewTest& operator=(const ManagePasswordsIconViewTest&) =
      delete;

  ~ManagePasswordsIconViewTest() override = default;

  password_manager::ui::State ViewState() {
    return GetController()->GetState();
  }

  IconLabelBubbleView* GetIcon() {
    auto* view = BrowserView::GetBrowserViewForBrowser(browser())
                     ->toolbar_button_provider()
                     ->GetPageActionView(kActionShowPasswordsBubbleOrPage);
    return view;
  }

  std::u16string GetTooltipText() { return GetIcon()->GetTooltipText(); }

  bool IsMigrationEnabled() const { return GetParam(); }

  static std::string GetTestSuffix(const testing::TestParamInfo<bool>& info) {
    return info.param ? "MigrationOn" : "MigrationOff";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ManagePasswordsIconViewTestToolbarPinningOnly
    : public ManagePasswordsIconViewTest {
 protected:
  raw_ptr<content::WebContents> GetActiveWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
  bool IsBubbleShowing() const {
    return PasswordBubbleViewBase::manage_password_bubble() &&
           PasswordBubbleViewBase::manage_password_bubble()
               ->GetWidget()
               ->IsVisible();
  }
};

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTest, DefaultStateIsInactive) {
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, ViewState());
  EXPECT_FALSE(GetIcon()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTest, PendingState) {
  SetupPendingPassword();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, ViewState());
  EXPECT_TRUE(GetIcon()->GetVisible());
  // No tooltip because the bubble is showing.
  EXPECT_EQ(std::u16string(), GetTooltipText());
}

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTest, ManageState) {
  SetupManagingPasswords();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, ViewState());
  EXPECT_TRUE(GetIcon()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE),
            GetTooltipText());
}

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTest, CloseOnClick) {
  SetupPendingPassword();
  EXPECT_TRUE(GetIcon()->GetVisible());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      static_cast<views::Button*>(GetIcon()),
      ui::test::InteractionTestUtil::InputType::kDontCare);
  // Wait for the command execution to close the bubble.
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTestToolbarPinningOnly,
                       ShowPasswordsBubbleOrPage) {
  const GURL passwords_url = GURL("chrome://password-manager/");
  PinnedToolbarActionsModel::Get(browser()->profile())
      ->UpdatePinnedState(kActionShowPasswordsBubbleOrPage, true);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  PinnedActionToolbarButton* button =
      browser_view->toolbar()->pinned_toolbar_actions_container()->GetButtonFor(
          kActionShowPasswordsBubbleOrPage);
  ASSERT_NE(button, nullptr);

  // Underline should not be visible here.
  EXPECT_EQ(button->GetStatusIndicatorForTesting()->GetVisible(), false);

  SetupManagingPasswords();
  ASSERT_FALSE(IsBubbleShowing());

  // Underline should show in this case.
  EXPECT_EQ(button->GetStatusIndicatorForTesting()->GetVisible(), true);

  views::test::InteractionTestUtilSimulatorViews::PressButton(
      button, ui::test::InteractionTestUtil::InputType::kDontCare);
  EXPECT_TRUE(IsBubbleShowing());

  AddBlankTabAndShow(browser());
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      button, ui::test::InteractionTestUtil::InputType::kDontCare);
  EXPECT_EQ(GetActiveWebContents()->GetVisibleURL(), passwords_url);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ManagePasswordsIconViewTest,
                         ::testing::Bool(),
                         &ManagePasswordsIconViewTest::GetTestSuffix);

INSTANTIATE_TEST_SUITE_P(All,
                         ManagePasswordsIconViewTestToolbarPinningOnly,
                         ::testing::Bool(),
                         &ManagePasswordsIconViewTest::GetTestSuffix);
