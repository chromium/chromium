// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
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

class ManagePasswordsIconViewTest : public ManagePasswordsTest {
 public:
  ManagePasswordsIconViewTest() = default;

  ManagePasswordsIconViewTest(const ManagePasswordsIconViewTest&) = delete;
  ManagePasswordsIconViewTest& operator=(const ManagePasswordsIconViewTest&) =
      delete;

  ~ManagePasswordsIconViewTest() override = default;

  password_manager::ui::State ViewState() { return GetView()->state_; }

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

  gfx::ImageSkia GetImage() { return GetView()->GetImageView()->GetImage(); }
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, DefaultStateIsInactive) {
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, ViewState());
  EXPECT_FALSE(GetView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, PendingState) {
  SetupPendingPassword();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, ViewState());
  EXPECT_TRUE(GetView()->GetVisible());
  // No tooltip because the bubble is showing.
  EXPECT_EQ(std::u16string(), GetTooltipText());
  const gfx::ImageSkia active_image = GetImage();
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, ManageState) {
  SetupManagingPasswords();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, ViewState());
  EXPECT_TRUE(GetView()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE),
            GetTooltipText());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, CloseOnClick) {
  SetupPendingPassword();
  EXPECT_TRUE(GetView()->GetVisible());
  ui::MouseEvent mouse_down(ui::ET_MOUSE_PRESSED, gfx::Point(10, 10),
                            gfx::Point(900, 60), ui::EventTimeForNow(),
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  GetView()->OnMousePressed(mouse_down);
  // Wait for the command execution to close the bubble.
  content::RunAllPendingInMessageLoop();
}
