// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_utils.h"

// The param indicates if the feature showing password icon in the new toolbar
// status chip is enabled.
class ManagePasswordsIconViewTest : public ManagePasswordsTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  ManagePasswordsIconViewTest() {}
  ~ManagePasswordsIconViewTest() override {}

  password_manager::ui::State ViewState() { return GetView()->state_; }

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          autofill::features::kAutofillEnableToolbarStatusChip);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          autofill::features::kAutofillEnableToolbarStatusChip);
    }
    ManagePasswordsTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ManagePasswordsTest::SetUpOnMainThread();
    ReduceAnimationTime();
  }

  ManagePasswordsIconViews* GetView() {
    views::View* const view =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kManagePasswords);
    DCHECK(views::IsViewClass<ManagePasswordsIconViews>(view));
    return static_cast<ManagePasswordsIconViews*>(view);
  }

  std::u16string GetTooltipText() {
    return GetView()->GetTooltipText(gfx::Point());
  }

  const gfx::ImageSkia& GetImage() {
    return GetView()->GetImageView()->GetImage();
  }

  void WaitForAnimationToEnd() {
    auto* const animating_layout = GetAnimatingLayoutManager();
    if (animating_layout)
      views::test::WaitForAnimatingLayoutManager(animating_layout);
  }

 private:
  views::AnimatingLayoutManager* GetAnimatingLayoutManager() {
    if (!GetParam())
      return nullptr;
    return views::test::GetAnimatingLayoutManager(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->toolbar_account_icon_container());
  }

  void ReduceAnimationTime() {
    auto* const animating_layout = GetAnimatingLayoutManager();
    if (animating_layout) {
      animating_layout->SetAnimationDuration(
          base::TimeDelta::FromMilliseconds(1));
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconViewTest);
};

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTest, DefaultStateIsInactive) {
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, ViewState());
  WaitForAnimationToEnd();
  EXPECT_FALSE(GetView()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTest, PendingState) {
  SetupPendingPassword();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, ViewState());
  WaitForAnimationToEnd();
  EXPECT_TRUE(GetView()->GetVisible());
  // No tooltip because the bubble is showing.
  EXPECT_EQ(std::u16string(), GetTooltipText());
  const gfx::ImageSkia active_image = GetImage();
}

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTest, ManageState) {
  SetupManagingPasswords();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, ViewState());
  WaitForAnimationToEnd();
  EXPECT_TRUE(GetView()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE),
            GetTooltipText());
}

IN_PROC_BROWSER_TEST_P(ManagePasswordsIconViewTest, CloseOnClick) {
  SetupPendingPassword();
  WaitForAnimationToEnd();
  EXPECT_TRUE(GetView()->GetVisible());
  ui::MouseEvent mouse_down(ui::ET_MOUSE_PRESSED, gfx::Point(10, 10),
                            gfx::Point(900, 60), ui::EventTimeForNow(),
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  GetView()->OnMousePressed(mouse_down);
  // Wait for the command execution to close the bubble.
  content::RunAllPendingInMessageLoop();
}

// TODO(crbug.com/932818): Remove the condition once the experiment is enabled
// on ChromeOS. For now, on ChromeOS, we only test the non-experimental branch.
#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_TEST_SUITE_P(All,
                         ManagePasswordsIconViewTest,
                         ::testing::Values(false));
#else
INSTANTIATE_TEST_SUITE_P(All, ManagePasswordsIconViewTest, ::testing::Bool());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
