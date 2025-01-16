// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_icon_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/interaction/interaction_test_util_views.h"

using testing::Return;

class PasswordChangeIconViewsTest : public ManagePasswordsTest {
 public:
 public:
  PasswordChangeIconViewsTest() = default;

  PasswordChangeIconViewsTest(const PasswordChangeIconViewsTest&) = delete;
  PasswordChangeIconViewsTest& operator=(const PasswordChangeIconViewsTest&) =
      delete;

  ~PasswordChangeIconViewsTest() override = default;

  PasswordChangeIconViews* GetView() {
    views::View* const view =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kChangePassword);
    CHECK(views::IsViewClass<PasswordChangeIconViews>(view));
    return static_cast<PasswordChangeIconViews*>(view);
  }

  std::u16string GetTooltipText() {
    return GetView()->GetTooltipText(gfx::Point());
  }
};

IN_PROC_BROWSER_TEST_F(PasswordChangeIconViewsTest,
                       ViewIsNotVisibleWhenManagingPasswords) {
  SetupManagingPasswords();
  EXPECT_FALSE(GetView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(PasswordChangeIconViewsTest,
                       ViewIsVisibleWhenChangingPassword) {
  SetupPasswordChange();
  EXPECT_TRUE(GetView()->GetVisible());
}
