// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"

using web_app::AppId;

namespace {

AppId InstallTestWebApp(Profile* profile) {
  const GURL example_url = GURL("http://example.org/");

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = example_url;
  web_app_info->scope = example_url;
  web_app_info->open_as_window = true;
  return web_app::InstallWebApp(profile, std::move(web_app_info));
}

}  // namespace

using WebAppUninstallDialogViewBrowserTest = InProcessBrowserTest;

// Test that WebAppUninstallDialog cancels the uninstall if the Window
// which is passed to WebAppUninstallDialog::Create() is destroyed before
// WebAppUninstallDialogDelegateView is created.
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TrackParentWindowDestruction) {
  AppId app_id = InstallTestWebApp(browser()->profile());

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog(
      web_app::WebAppUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow()));

  browser()->window()->Close();
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  bool was_uninstalled = false;
  dialog->ConfirmUninstall(app_id,
                           base::BindLambdaForTesting([&](bool uninstalled) {
                             was_uninstalled = uninstalled;
                             run_loop.Quit();
                           }));
  run_loop.Run();
  EXPECT_FALSE(was_uninstalled);
}

// Test that WebAppUninstallDialog cancels the uninstall if the Window
// which is passed to WebAppUninstallDialog::Create() is destroyed after
// WebAppUninstallDialogDelegateView is created.
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TrackParentWindowDestructionAfterViewCreation) {
  AppId app_id = InstallTestWebApp(browser()->profile());

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog(
      web_app::WebAppUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow()));
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  bool was_uninstalled = false;
  dialog->ConfirmUninstall(app_id,
                           base::BindLambdaForTesting([&](bool uninstalled) {
                             was_uninstalled = uninstalled;
                             run_loop.Quit();
                           }));

  // Kill parent window.
  browser()->window()->Close();
  run_loop.Run();
  EXPECT_FALSE(was_uninstalled);
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TestDialogUserFlow_Cancel) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);
  AppId app_id = InstallTestWebApp(browser()->profile());

  WebAppUninstallDialogViews dialog(browser()->profile(),
                                    browser()->window()->GetNativeWindow());

  base::RunLoop run_loop;
  auto callback =
      base::BarrierClosure(/*num_closures=*/2, run_loop.QuitClosure());
  bool was_uninstalled = false;

  dialog.SetDialogShownCallbackForTesting(callback);
  dialog.ConfirmUninstall(app_id,
                          base::BindLambdaForTesting([&](bool uninstalled) {
                            was_uninstalled = uninstalled;
                            callback.Run();
                          }));
  run_loop.Run();
  EXPECT_FALSE(was_uninstalled);
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TestDialogUserFlow_Accept) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION);
  AppId app_id = InstallTestWebApp(browser()->profile());

  WebAppUninstallDialogViews dialog(browser()->profile(),
                                    browser()->window()->GetNativeWindow());

  base::RunLoop run_loop;
  auto callback =
      base::BarrierClosure(/*num_closures=*/2, run_loop.QuitClosure());
  bool was_uninstalled = false;

  dialog.SetDialogShownCallbackForTesting(callback);
  dialog.ConfirmUninstall(app_id,
                          base::BindLambdaForTesting([&](bool uninstalled) {
                            was_uninstalled = uninstalled;
                            callback.Run();
                          }));

  run_loop.Run();
  EXPECT_TRUE(was_uninstalled);
}

#if defined(OS_CHROMEOS)
// Test that we don't crash when uninstalling a web app from a web app window in
// Ash. Context: crbug.com/825554
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       WebAppWindowAshCrash) {
  AppId app_id = InstallTestWebApp(browser()->profile());
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog;
  {
    base::RunLoop run_loop;
    dialog = web_app::WebAppUninstallDialog::Create(
        app_browser->profile(), app_browser->window()->GetNativeWindow());
    run_loop.RunUntilIdle();
  }

  {
    base::RunLoop run_loop;
    dialog->ConfirmUninstall(app_id, base::DoNothing());
    run_loop.RunUntilIdle();
  }
}
#endif  // defined(OS_CHROMEOS)

class WebAppUninstallDialogViewInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    AppId app_id = InstallTestWebApp(browser()->profile());

    dialog_ = web_app::WebAppUninstallDialog::Create(
        browser()->profile(), browser()->window()->GetNativeWindow());

    base::RunLoop run_loop;
    dialog_->SetDialogShownCallbackForTesting(run_loop.QuitClosure());

    dialog_->ConfirmUninstall(app_id, base::DoNothing());

    run_loop.Run();
  }

 private:
  void TearDownOnMainThread() override {
    // Dialog holds references to the profile, so it needs to tear down before
    // profiles are deleted.
    dialog_.reset();
  }

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog_;
};

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ManualUninstall) {
  ShowAndVerifyUi();
}
