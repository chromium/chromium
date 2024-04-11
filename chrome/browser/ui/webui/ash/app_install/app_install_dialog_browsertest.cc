// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace ash::app_install {

namespace {

content::WebContents* GetWebContentsFromDialog() {
  ash::SystemWebDialogDelegate* dialog =
      ash::SystemWebDialogDelegate::FindInstance(
          chrome::kChromeUIAppInstallDialogURL);
  EXPECT_TRUE(dialog);
  content::WebUI* webui = dialog->GetWebUIForTest();
  EXPECT_TRUE(webui);
  content::WebContents* web_contents = webui->GetWebContents();
  EXPECT_TRUE(web_contents);
  return web_contents;
}

}  // namespace

class AppInstallDialogBrowserTest : public InProcessBrowserTest {
 public:
  AppInstallDialogBrowserTest() {
    feature_list_.InitWithFeatures(
        {chromeos::features::kCrosWebAppInstallDialog,
         chromeos::features::kCrosOmniboxInstallDialog},
        {});
  }

  std::string GetTitle(content::WebContents* web_contents) {
    return content::EvalJs(web_contents, R"(
      document.querySelector('app-install-dialog').shadowRoot
              .querySelector('#title').textContent
    )")
        .ExtractString();
  }

  std::string GetActionButton(content::WebContents* web_contents) {
    return content::EvalJs(web_contents, R"(
      document.querySelector('app-install-dialog').shadowRoot
              .querySelector('.action-button').label
    )")
        .ExtractString();
  }

  [[nodiscard]] bool ClickActionButton(content::WebContents* web_contents) {
    return content::ExecJs(web_contents, R"(
      document.querySelector('app-install-dialog').shadowRoot
              .querySelector('.action-button').click();
    )");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppInstallDialogBrowserTest, InstallApp) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL("/web_apps/basic.html"));

  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), app_url, 1, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUIAppInstallDialogURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  web_app::CreateWebAppFromCurrentWebContents(
      browser(), web_app::WebAppInstallFlow::kCreateShortcut);

  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromDialog();

  EXPECT_TRUE(base::StartsWith(GetTitle(web_contents), "Install app on your"));

  // Click the install button.
  EXPECT_TRUE(ClickActionButton(web_contents));

  // Make sure the button goes through the 'Installing' state.
  while (GetActionButton(web_contents) != "Installing")
    ;
  EXPECT_TRUE(base::StartsWith(GetTitle(web_contents), "Installing app"));

  // Wait for the button text to say "Open app", which means it knows the app
  // was installed successfully.
  while (GetActionButton(web_contents) != "Open app")
    ;
  EXPECT_EQ(GetTitle(web_contents), "App installed");

  // Click the open app button and expect the dialog was closed.
  content::WebContentsDestroyedWatcher watcher(web_contents);
  EXPECT_TRUE(ClickActionButton(web_contents));
  watcher.Wait();

  // Expect the app is opened.
  webapps::AppId app_id = web_app::GenerateAppIdFromManifestId(app_url);
  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));

  // Expect the browser tab was not closed.
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      app_url);
}

IN_PROC_BROWSER_TEST_F(AppInstallDialogBrowserTest, FailedInstall) {
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUIAppInstallDialogURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  base::WeakPtr<AppInstallDialog> dialog_handle =
      AppInstallDialog::CreateDialog();

  // TODO(b/331310950): Add a test that sends a retry callback.
  auto args = ash::app_install::mojom::DialogArgs::New();
  args->url = GURL("https://example.org");
  dialog_handle->ShowApp(
      browser()->profile(), browser()->window()->GetNativeWindow(),
      /* dialog_args= */ std::move(args),
      /* icon_width= */ 0, /* is_icon_maskable= */ true,
      /* expected_app_id= */ "",
      base::BindOnce(
          [](base::WeakPtr<AppInstallDialog> dialog_handle,
             bool dialog_accepted) {
            dialog_handle->SetInstallFailed(base::DoNothing());
          },
          dialog_handle));

  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromDialog();

  // Click the install button.
  EXPECT_TRUE(ClickActionButton(web_contents));

  // Make sure the button goes through the 'Installing' state.
  while (GetActionButton(web_contents) != "Installing")
    ;

  // Wait for the button text to say "Try again".
  while (GetActionButton(web_contents) != "Try again")
    ;

  EXPECT_EQ(GetTitle(web_contents), "Can't install app. Something went wrong.");
}

IN_PROC_BROWSER_TEST_F(AppInstallDialogBrowserTest, NoAppError) {
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUIAppInstallDialogURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
  proxy->AppInstallService().InstallApp(
      apps::AppInstallSurface::kAppInstallUriUnknown,
      apps::PackageId(apps::PackageType::kWeb, "invalid"),
      /*anchor_window=*/std::nullopt,
      /*callback=*/base::DoNothing());

  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromDialog();

  EXPECT_EQ(GetTitle(web_contents), "Could not download app data");
  EXPECT_EQ(GetActionButton(web_contents), "Try again");
}

}  // namespace ash::app_install
