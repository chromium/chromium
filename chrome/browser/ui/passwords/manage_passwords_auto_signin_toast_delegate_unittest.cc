// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_auto_signin_toast_delegate.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/test/mock_toast_controller.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/models/menu_model.h"

namespace {

using testing::_;
using testing::Return;

class MockManagePasswordsAutoSigninToastDelegate
    : public ManagePasswordsAutoSigninToastDelegate {
 public:
  using ManagePasswordsAutoSigninToastDelegate::
      ManagePasswordsAutoSigninToastDelegate;

  MOCK_METHOD(ToastController*, GetToastController, (), (override));
  MOCK_METHOD(void,
              NavigateToPasswordManagerSettings,
              (BrowserWindowInterface*),
              (override));
};

class ManagePasswordsAutoSigninToastDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mock_tab_interface_ = std::make_unique<tabs::MockTabInterface>();

    // Attach MockTabInterface to the active WebContents
    tabs::TabLookupFromWebContents::CreateForWebContents(
        web_contents(), mock_tab_interface_.get());
  }

 protected:
  std::unique_ptr<tabs::MockTabInterface> mock_tab_interface_;
};

TEST_F(ManagePasswordsAutoSigninToastDelegateTest, ExecuteCommand) {
  MockManagePasswordsAutoSigninToastDelegate delegate(web_contents());

  // We need a dummy BWI pointer to verify it's passed through.
  // It doesn't need to be functional since we mock the consumption.
  MockBrowserWindowInterface mock_browser_window_interface;

  // Setup expectations
  EXPECT_CALL(*mock_tab_interface_, GetBrowserWindowInterface())
      .WillOnce(Return(&mock_browser_window_interface));

  EXPECT_CALL(delegate,
              NavigateToPasswordManagerSettings(&mock_browser_window_interface))
      .WillOnce(Return());

  delegate.ExecuteCommand(ManagePasswordsAutoSigninToastDelegate::
                              kAutoSignInOpenPasswordManagerSettingsCommand,
                          0);
}

TEST_F(ManagePasswordsAutoSigninToastDelegateTest, OnAutoSignInToast) {
  const std::u16string kUsername = u"test_user";
  MockManagePasswordsAutoSigninToastDelegate delegate(web_contents());
  MockBrowserWindowInterface mock_browser_window_interface;
  auto mock_toast_controller =
      std::make_unique<MockToastController>(&mock_browser_window_interface);

  EXPECT_CALL(delegate, GetToastController())
      .WillRepeatedly(Return(mock_toast_controller.get()));

  EXPECT_CALL(*mock_toast_controller, MaybeShowToast(_))
      .WillOnce([kUsername](ToastParams params) {
        EXPECT_EQ(params.toast_id, ToastId::kAutoSignIn);
        EXPECT_EQ(params.body_string_replacement_params.size(), 1u);
        EXPECT_EQ(params.body_string_replacement_params[0], kUsername);
        EXPECT_NE(params.menu_model, nullptr);
        return true;
      });

  delegate.OnAutoSignInToast(kUsername);
}

TEST_F(ManagePasswordsAutoSigninToastDelegateTest, MenuModelExecutesCommand) {
  const std::u16string kUsername = u"test_user";
  MockManagePasswordsAutoSigninToastDelegate delegate(web_contents());
  MockBrowserWindowInterface mock_browser_window_interface;
  auto mock_toast_controller =
      std::make_unique<MockToastController>(&mock_browser_window_interface);

  EXPECT_CALL(delegate, GetToastController())
      .WillRepeatedly(Return(mock_toast_controller.get()));

  std::unique_ptr<ui::MenuModel> captured_menu_model;
  EXPECT_CALL(*mock_toast_controller, MaybeShowToast(_))
      .WillOnce([&captured_menu_model](ToastParams params) {
        captured_menu_model = std::move(params.menu_model);
        return true;
      });

  delegate.OnAutoSignInToast(kUsername);
  ASSERT_NE(captured_menu_model, nullptr);
  ASSERT_EQ(captured_menu_model->GetItemCount(), 1u);

  EXPECT_CALL(*mock_tab_interface_, GetBrowserWindowInterface())
      .WillOnce(Return(&mock_browser_window_interface));
  EXPECT_CALL(delegate,
              NavigateToPasswordManagerSettings(&mock_browser_window_interface))
      .WillOnce(Return());

  captured_menu_model->ActivatedAt(0);
}

TEST_F(ManagePasswordsAutoSigninToastDelegateTest,
       MenuModelHandlesDestroyedDelegate) {
  const std::u16string kUsername = u"test_user";
  auto delegate = std::make_unique<MockManagePasswordsAutoSigninToastDelegate>(
      web_contents());
  MockBrowserWindowInterface mock_browser_window_interface;
  auto mock_toast_controller =
      std::make_unique<MockToastController>(&mock_browser_window_interface);

  EXPECT_CALL(*delegate, GetToastController())
      .WillRepeatedly(Return(mock_toast_controller.get()));

  std::unique_ptr<ui::MenuModel> captured_menu_model;
  EXPECT_CALL(*mock_toast_controller, MaybeShowToast(_))
      .WillOnce([&captured_menu_model](ToastParams params) {
        captured_menu_model = std::move(params.menu_model);
        return true;
      });

  delegate->OnAutoSignInToast(kUsername);
  ASSERT_NE(captured_menu_model, nullptr);

  // Destroy the tab-scoped delegate and WebContents while the window-scoped
  // toast still owns the menu model (e.g. when the originating tab closes while
  // the menu is open).
  delegate.reset();
  DeleteContents();

  EXPECT_CALL(*mock_tab_interface_, GetBrowserWindowInterface()).Times(0);
  captured_menu_model->MenuWillShow();
  captured_menu_model->ActivatedAt(0);
  captured_menu_model->MenuWillClose();

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(ManagePasswordsAutoSigninToastDelegateTest,
       ExecuteCommandHandlesDestroyedWebContents) {
  MockManagePasswordsAutoSigninToastDelegate delegate(web_contents());
  DeleteContents();

  EXPECT_CALL(*mock_tab_interface_, GetBrowserWindowInterface()).Times(0);
  EXPECT_CALL(delegate, NavigateToPasswordManagerSettings(_)).Times(0);

  delegate.ExecuteCommand(ManagePasswordsAutoSigninToastDelegate::
                              kAutoSignInOpenPasswordManagerSettingsCommand,
                          0);
}

}  // namespace
