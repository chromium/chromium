// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
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
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
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
#include "ui/views/widget/any_widget_observer.h"

using web_app::AppId;

namespace {

AppId InstallTestWebApp(Profile* profile) {
  const GURL example_url = GURL("http://example.org/");

  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->start_url = example_url;
  web_app_info->scope = example_url;
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

}  // namespace

class WebAppUninstallDialogViewBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  web_app::WebAppProvider* provider() {
    return web_app::WebAppProvider::GetForTest(browser()->profile());
  }

 private:
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TrackParentWindowDestructionAfterViewCreation) {
  AppId app_id = InstallTestWebApp(browser()->profile());

  web_app::WebAppUiManagerImpl* ui_manager_impl =
      web_app::WebAppUiManagerImpl::Get(provider());
  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  ui_manager_impl->PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      browser()->window()->GetNativeWindow(), test_future.GetCallback());

  views::NamedWidgetShownWaiter uninstall_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppUninstallDialogDelegateView");
  auto* uninstall_widget = uninstall_dialog_waiter.WaitIfNeededAndGet();
  EXPECT_NE(uninstall_widget, nullptr);

  // Kill parent window.
  browser()->window()->Close();
  EXPECT_TRUE(test_future.Wait());
  EXPECT_EQ(test_future.Get<webapps::UninstallResultCode>(),
            webapps::UninstallResultCode::kCancelled);
}

// Uninstalling with no browser window open can cause the view to be destroyed
// before the views object. Test that this does not cause a UAF. See
// https://crbug.com/1150798.
// Also tests that we don't crash when uninstalling a web app from a web app
// window in Ash. See https://crbug.com/825554.
IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TrackParentWindowDestructionBeforeViewCreation) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  AppId app_id = InstallTestWebApp(browser()->profile());
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  chrome::CloseWindow(browser());

  web_app::WebAppUiManagerImpl* ui_manager_impl =
      web_app::WebAppUiManagerImpl::Get(provider());
  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  ui_manager_impl->PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      app_browser->window()->GetNativeWindow(), test_future.GetCallback());

  EXPECT_TRUE(test_future.Wait());
  EXPECT_EQ(test_future.Get<webapps::UninstallResultCode>(),
            webapps::UninstallResultCode::kSuccess);
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TestDialogUserFlow_Cancel) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);
  AppId app_id = InstallTestWebApp(browser()->profile());

  web_app::WebAppUiManagerImpl* ui_manager_impl =
      web_app::WebAppUiManagerImpl::Get(provider());
  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  ui_manager_impl->PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      browser()->window()->GetNativeWindow(), test_future.GetCallback());

  EXPECT_TRUE(test_future.Wait());
  EXPECT_EQ(test_future.Get<webapps::UninstallResultCode>(),
            webapps::UninstallResultCode::kCancelled);
}

IN_PROC_BROWSER_TEST_F(WebAppUninstallDialogViewBrowserTest,
                       TestDialogUserFlow_Accept) {
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION);
  AppId app_id = InstallTestWebApp(browser()->profile());

  web_app::WebAppUiManagerImpl* ui_manager_impl =
      web_app::WebAppUiManagerImpl::Get(provider());
  base::test::TestFuture<webapps::UninstallResultCode> test_future;
  ui_manager_impl->PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      browser()->window()->GetNativeWindow(), test_future.GetCallback());

  EXPECT_TRUE(test_future.Wait());
  EXPECT_EQ(test_future.Get<webapps::UninstallResultCode>(),
            webapps::UninstallResultCode::kSuccess);
}
