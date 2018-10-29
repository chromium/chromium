// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"

class ManagePasswordsIconViewTest : public ManagePasswordsTest {
 public:
  ManagePasswordsIconViewTest() {}
  ~ManagePasswordsIconViewTest() override {}

  password_manager::ui::State ViewState() { return GetView()->state_; }

  ManagePasswordsIconViews* GetView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar()
        ->manage_passwords_icon_view();
  }

  base::string16 GetTooltipText() {
    base::string16 tooltip;
    GetView()->GetTooltipText(gfx::Point(), &tooltip);
    return tooltip;
  }

  const gfx::ImageSkia& GetImage() {
    return GetView()->GetImageView()->GetImage();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconViewTest);
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, DefaultStateIsInactive) {
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, ViewState());
  EXPECT_FALSE(GetView()->visible());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, PendingState) {
  SetupPendingPassword();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, ViewState());
  EXPECT_TRUE(GetView()->visible());
  // No tooltip because the bubble is showing.
  EXPECT_EQ(base::string16(), GetTooltipText());
  const gfx::ImageSkia active_image = GetImage();
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, ManageState) {
  SetupManagingPasswords();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, ViewState());
  EXPECT_TRUE(GetView()->visible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE),
            GetTooltipText());
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsIconViewTest, CloseOnClick) {
  SetupPendingPassword();
  EXPECT_TRUE(GetView()->visible());
  ui::MouseEvent mouse_down(ui::ET_MOUSE_PRESSED, gfx::Point(10, 10),
                            gfx::Point(900, 60), ui::EventTimeForNow(),
                            ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  GetView()->OnMousePressed(mouse_down);
  // Wait for the command execution to close the bubble.
  content::RunAllPendingInMessageLoop();
}
