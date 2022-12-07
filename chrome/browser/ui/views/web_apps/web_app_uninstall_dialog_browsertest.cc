// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_uninstall_dialog_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"

using web_app::AppId;

namespace {

AppId InstallTestWebApp(Profile* profile) {
  const GURL example_url = GURL("http://example.org/");

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->start_url = example_url;
  web_app_info->scope = example_url;
  web_app_info->user_display_mode = web_app::UserDisplayMode::kStandalone;
  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

}  // namespace

class WebAppUninstallDialogViewBrowserTest : public InProcessBrowserTest {
 private:
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};

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
  dialog->ConfirmUninstall(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kCancelled);
        run_loop.Quit();
      }));
  run_loop.Run();
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1224161
#define MAYBE_TrackParentWindowDestructionAfterViewCreation \
  DISABLED_TrackParentWindowDestructionAfterViewCreation
#else
#define MAYBE_TrackParentWindowDestructionAfterViewCreation \
  TrackParentWindowDestructionAfterViewCreation
#endif
// Test that WebAppUninstallDialog cancels the uninstall if the Window
// which is passed to WebAppUninstallDialog::Create() is destroyed after
// WebAppUninstallDialogDelegateView is created.
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       MAYBE_TrackParentWindowDestructionAfterViewCreation) {
  AppId app_id = InstallTestWebApp(browser()->profile());

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog(
      web_app::WebAppUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow()));
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  dialog->ConfirmUninstall(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kCancelled);
        run_loop.Quit();
      }));

  // Kill parent window.
  browser()->window()->Close();
  run_loop.Run();
}

// Uninstalling with no browser window open can cause the view to be destroyed
// before the views object. Test that this does not cause a UAF.
// See https://crbug.com/1150798.
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       UninstallWithNoBrowserWindow) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  AppId app_id = InstallTestWebApp(browser()->profile());
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  chrome::CloseWindow(browser());

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog(
      web_app::WebAppUninstallDialog::Create(
          browser()->profile(), app_browser->window()->GetNativeWindow()));
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  dialog->ConfirmUninstall(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        run_loop.Quit();
      }));
  run_loop.Run();
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1224161
#define MAYBE_UninstallOnCancelShutdownBrowser \
  DISABLED_UninstallOnCancelShutdownBrowser
#else
#define MAYBE_UninstallOnCancelShutdownBrowser UninstallOnCancelShutdownBrowser
#endif
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       MAYBE_UninstallOnCancelShutdownBrowser) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);
  AppId app_id = InstallTestWebApp(browser()->profile());

  std::unique_ptr<web_app::WebAppUninstallDialog> dialog(
      web_app::WebAppUninstallDialog::Create(browser()->profile(),
                                             gfx::kNullNativeWindow));

  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  // ScopedKeepAlive is used by `UninstallWebAppWithDialogFromStartupSwitch`.
  // ScopedKeepAlive's destruction triggers browser shutdown when there is no
  // open window. This verifies the destruction doesn't race with the dialog
  // shutdown itself.
  std::unique_ptr<ScopedKeepAlive> scoped_keep_alive =
      std::make_unique<ScopedKeepAlive>(KeepAliveOrigin::WEB_APP_UNINSTALL,
                                        KeepAliveRestartOption::DISABLED);

  chrome::CloseWindow(browser());

  dialog->ConfirmUninstall(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kCancelled);
        scoped_keep_alive.reset();
        run_loop.Quit();
      }));
  run_loop.Run();
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

  dialog.SetDialogShownCallbackForTesting(callback);
  dialog.ConfirmUninstall(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kCancelled);
        callback.Run();
      }));
  run_loop.Run();
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

  dialog.SetDialogShownCallbackForTesting(callback);
  dialog.ConfirmUninstall(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        callback.Run();
      }));

  run_loop.Run();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test that we don't crash when uninstalling a web app from a web app window in
// Ash. Context: crbug.com/825554
// TODO(crbug.com/1332923): The test is flaky.
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       DISABLED_WebAppWindowAshCrash) {
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
    dialog->ConfirmUninstall(app_id, webapps::WebappUninstallSource::kAppMenu,
                             base::DoNothing());
    run_loop.RunUntilIdle();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WebAppUninstallDialogViewInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    AppId app_id = InstallTestWebApp(browser()->profile());

    dialog_ = web_app::WebAppUninstallDialog::Create(
        browser()->profile(), browser()->window()->GetNativeWindow());

    base::RunLoop run_loop;
    dialog_->SetDialogShownCallbackForTesting(run_loop.QuitClosure());

    dialog_->ConfirmUninstall(app_id, webapps::WebappUninstallSource::kAppMenu,
                              base::DoNothing());

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
