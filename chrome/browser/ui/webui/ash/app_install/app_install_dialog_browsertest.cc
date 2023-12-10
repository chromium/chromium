// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
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
    feature_list_.InitAndEnableFeature(
        chromeos::features::kCrosWebAppInstallDialog);
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

  // Click the install button.
  EXPECT_TRUE(
      content::ExecJs(web_contents,
                      "document.querySelector('app-install-dialog')."
                      "shadowRoot.querySelector('.action-button').click()"));

  // Wait for the button text to say "Open app", which means it knows the app
  // was installed successfully.
  while (
      !content::EvalJs(
           web_contents,
           "document.querySelector('app-install-dialog').shadowRoot."
           "querySelector('.action-button').textContent.includes('Open app')")
           .ExtractBool()) {
  }
}

IN_PROC_BROWSER_TEST_F(AppInstallDialogBrowserTest, FailedInstall) {
  content::TestNavigationObserver navigation_observer_dialog(
      (GURL(chrome::kChromeUIAppInstallDialogURL)));
  navigation_observer_dialog.StartWatchingNewWebContents();

  base::WeakPtr<AppInstallDialog> dialog_handle =
      AppInstallDialog::CreateDialog();

  dialog_handle->Show(
      browser()->window()->GetNativeWindow(),
      ChromeOsAppInstallDialogParams(
          *web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
              GURL("https://example.com/")),
          std::vector<webapps::Screenshot>()),
      base::BindOnce(
          [](base::WeakPtr<AppInstallDialog> dialog_handle,
             bool dialog_accepted) { dialog_handle->SetInstallSuccess(false); },
          dialog_handle));

  navigation_observer_dialog.Wait();
  ASSERT_TRUE(navigation_observer_dialog.last_navigation_succeeded());

  content::WebContents* web_contents = GetWebContentsFromDialog();

  // Click the install button.
  EXPECT_TRUE(
      content::ExecJs(web_contents,
                      "document.querySelector('app-install-dialog')."
                      "shadowRoot.querySelector('.action-button').click()"));

  // Make sure the button goes through the 'Installing' state.
  while (
      !content::EvalJs(
           web_contents,
           "document.querySelector('app-install-dialog').shadowRoot."
           "querySelector('.action-button').textContent.includes('Installing')")
           .ExtractBool()) {
  }

  // Wait for the button text to say "Install", which means it knows the install
  // has failed.
  while (!content::EvalJs(
              web_contents,
              "text = document.querySelector('app-install-dialog').shadowRoot."
              "querySelector('.action-button').textContent;"
              "text.includes('Install') && !text.includes('Installing')")
              .ExtractBool()) {
  }
}

}  // namespace ash::app_install
