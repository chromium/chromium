// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"

#include <memory>

#include "chrome/browser/password_manager/password_change_delegate_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/password_change_ui_controller.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTestChangePasswordUrl[] = "https://example.com/change_password";
const std::u16string kTestUsername = u"testuser";
const std::u16string kTestPassword = u"password";

class PasswordChangeToastBrowserTest : public UiBrowserTest {
 public:
  PasswordChangeToastBrowserTest() = default;
  ~PasswordChangeToastBrowserTest() override = default;

  PasswordChangeToastBrowserTest(const PasswordChangeToastBrowserTest&) =
      delete;
  PasswordChangeToastBrowserTest& operator=(
      const PasswordChangeToastBrowserTest&) = delete;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    tabs::TabInterface* tab_interface = browser()->GetActiveTabInterface();
    ASSERT_TRUE(tab_interface);

    password_manager::PasswordForm form;
    form.url = GURL(kTestChangePasswordUrl);
    form.signon_realm = GURL(kTestChangePasswordUrl).GetWithEmptyPath().spec();
    form.username_value = kTestUsername;
    form.password_value = kTestPassword;
    delegate_ = std::make_unique<PasswordChangeDelegateImpl>(
        GURL(kTestChangePasswordUrl), std::move(form), tab_interface);

    ui_controller_ = delegate_->ui_controller();

    // Trigger the toast for the "Password Changed" state.
    ui_controller_->UpdateState(
        PasswordChangeDelegate::State::kPasswordSuccessfullyChanged);
  }

  bool VerifyUi() override {
    views::Widget* widget = ui_controller_->toast_view()->GetWidget();

    auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "PasswordChangeToastBrowserTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

  void DismissUi() override {
    ui_controller_ = nullptr;
    delegate_ = nullptr;
  }

  void WaitForUserDismissal() override {
    ui_test_utils::WaitForBrowserToClose(browser());
  }

 private:
  std::unique_ptr<PasswordChangeDelegateImpl> delegate_;
  raw_ptr<PasswordChangeUIController> ui_controller_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeToastBrowserTest, InvokeUi_Toast) {
  ShowAndVerifyUi();
}

}  // namespace
