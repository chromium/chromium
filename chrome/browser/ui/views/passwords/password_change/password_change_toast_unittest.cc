// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/test/button_test_api.h"
#include "url/gurl.h"

class PasswordChangeToastTest : public ChromeViewsTestBase {
 public:
  PasswordChangeToastTest() = default;
  ~PasswordChangeToastTest() override = default;

  void TearDown() override {
    anchor_widget_ = nullptr;
    ChromeViewsTestBase::TearDown();
  }

  PasswordChangeToast* ShowToast(PasswordChangeToast::ToastOptions options) {
    auto toast_view = std::make_unique<PasswordChangeToast>(std::move(options));
    auto* raw_toast_view = toast_view.get();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_widget_->Show();
    anchor_widget_->SetContentsView(std::move(toast_view));
    return raw_toast_view;
  }

 private:
  std::unique_ptr<views::Widget> anchor_widget_;
};

TEST_F(PasswordChangeToastTest, DisplayedWithOptions) {
  PasswordChangeToast::ToastOptions options(u"Changing password...",
                                            base::DoNothing());
  auto* toast_view = ShowToast(std::move(options));
  EXPECT_TRUE(toast_view->throbber()->GetVisible());
  EXPECT_FALSE(toast_view->icon_view()->GetVisible());
  EXPECT_TRUE(toast_view->label()->GetVisible());
  EXPECT_EQ(u"Changing password...", toast_view->label()->GetText());
  EXPECT_FALSE(toast_view->action_button()->GetVisible());
  EXPECT_TRUE(toast_view->close_button()->GetVisible());
}

TEST_F(PasswordChangeToastTest, ConfigurationUpdated) {
  PasswordChangeToast::ToastOptions options(u"Checking sign in...",
                                            base::DoNothing());
  auto* toast_view = ShowToast(std::move(options));
  EXPECT_TRUE(toast_view->throbber()->GetVisible());
  EXPECT_FALSE(toast_view->icon_view()->GetVisible());
  EXPECT_TRUE(toast_view->label()->GetVisible());
  EXPECT_EQ(u"Checking sign in...", toast_view->label()->GetText());
  EXPECT_FALSE(toast_view->action_button()->GetVisible());
  EXPECT_TRUE(toast_view->close_button()->GetVisible());

  PasswordChangeToast::ToastOptions new_options(
      u"Password changed", vector_icons::kPasswordManagerIcon,
      base::DoNothing(), u"Details", base::DoNothing());
  toast_view->UpdateLayout(std::move(new_options));
  EXPECT_FALSE(toast_view->throbber()->GetVisible());
  EXPECT_TRUE(toast_view->icon_view()->GetVisible());
  EXPECT_TRUE(toast_view->label()->GetVisible());
  EXPECT_EQ(u"Password changed", toast_view->label()->GetText());
  EXPECT_TRUE(toast_view->action_button()->GetVisible());
  EXPECT_EQ(u"Details", toast_view->action_button()->GetText());
  EXPECT_TRUE(toast_view->close_button()->GetVisible());
}
