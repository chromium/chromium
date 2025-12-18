// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {
constexpr char kInstallElementId[] = "install-app";
}  // namespace

namespace web_app {

class InstallElementBrowserTest : public WebAppBrowserTestBase {
 public:
  InstallElementBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kInstallElement,
         blink::features::kBypassPepcSecurityForTesting},
        {});
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    console_observer_ =
        std::make_unique<content::WebContentsConsoleObserver>(web_contents());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // TODO(crbug.com/462477497): Do not show the permission prompt. Until this
  // is fixed, always accept all permission requests.
  void SetPermissionResponse() {
    permissions::PermissionRequestManager::FromWebContents(web_contents())
        ->set_auto_response_for_test(permissions::PermissionRequestManager::
                                         AutoResponseType::ACCEPT_ALL);
  }

  bool SetButtonInstallUrl(const GURL& install_url) {
    const std::string script =
        "document.getElementById('" + std::string(kInstallElementId) +
        "').setAttribute('installurl', '" + install_url.spec() + "');";
    return content::ExecJs(web_contents(), script);
  }

  bool SetButtonManifestId(const GURL& manifest_id) {
    const std::string script =
        "document.getElementById('" + std::string(kInstallElementId) +
        "').setAttribute('manifestid', '" + manifest_id.spec() + "');";
    return content::ExecJs(web_contents(), script);
  }

  // Simulates a click on an element with the given |id|.
  bool ClickElementWithId(const std::string& id,
                          content::WebContents* contents = nullptr) {
    const std::string script = "document.getElementById('" + id + "').click();";
    return content::ExecJs(contents ? contents : web_contents(), script);
  }

  void WaitForPromptActionEvent(const std::string& id) {
    ExpectConsoleMessage(id + "-promptaction");
  }

  void WaitForDismissEvent(const std::string& id) {
    ExpectConsoleMessage(id + "-promptdismiss");
  }

  // The web app test pages log quite a few additional console messages during
  // setup/load. Make sure that the set of received messages contains at least 1
  // instance of `expected_message`.
  void ExpectConsoleMessage(const std::string& expected_message) {
    EXPECT_TRUE(console_observer_->Wait());
    EXPECT_GE(console_observer_->messages().size(), 1u);

    bool found = false;
    for (const auto& message : console_observer_->messages()) {
      if (base::UTF16ToUTF8(message.message) == expected_message) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Expected console message not found: "
                       << expected_message;

    // Reset console observer to wait for next message.
    console_observer_ =
        std::make_unique<content::WebContentsConsoleObserver>(web_contents());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContentsConsoleObserver> console_observer_;
};

// Test installing current document (no attributes).
// <install></install>
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, Install) {
  // TODO(crbug.com/469831343): Fix race condition with <install></install>.
  // Replace this with a basic `NavigateToURL`.
  // Navigate to a page with <install> elements and wait for installability
  // checks to complete.
  EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(
      browser(), https_server()->GetURL("/web_apps/install_element/"
                                        "index.html")));

  // Setup test listeners and dialog auto-accepts.
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();

  // Click the install element and wait for the app to open.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ClickElementWithId(kInstallElementId);
  Browser* web_app_browser = browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app launched.
  ASSERT_TRUE(AppBrowserController::IsWebApp(web_app_browser));
  const WebAppBrowserController* app_controller =
      WebAppBrowserController::From(web_app_browser);
  EXPECT_EQ(app_controller->GetTitle(),
            u"Web app install element test app with id");

  // The registrar should now have one app installed.
  EXPECT_EQ(provider().registrar_unsafe().GetAppIds().size(), 1u);
}

// Test installing from a background document (installurl only).
// <install installurl="..."></install>
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, InstallWithUrl) {
  // Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL("/web_apps/install_element/index.html")));

  // Setup test listeners and dialog auto-accepts.
  SetPermissionResponse();
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();

  // Dynamically set the installurl attribute.
  // Since we're installing by URL only, the manifest must contain an id.
  const GURL install_url =
      https_server()->GetURL("/web_apps/custom_id/install_url.html");
  ASSERT_TRUE(SetButtonInstallUrl(install_url));

  // Click the install element and wait for the app to open.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ClickElementWithId(kInstallElementId);
  Browser* web_app_browser = browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app launched.
  ASSERT_TRUE(AppBrowserController::IsWebApp(web_app_browser));
  const WebAppBrowserController* app_controller =
      WebAppBrowserController::From(web_app_browser);
  EXPECT_EQ(app_controller->GetTitle(), u"Simple web app with a custom id");

  // The registrar should now have one app installed.
  EXPECT_EQ(provider().registrar_unsafe().GetAppIds().size(), 1u);
}

// Test installing from a background document (both installurl and manifestid).
// <install installurl="..." manifestid="..."></install>
IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, InstallWithUrlAndId) {
  // Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL("/web_apps/install_element/index.html")));

  // Setup test listeners and dialog auto-accepts.
  SetPermissionResponse();
  auto auto_accept_pwa_install_confirmation =
      SetAutoAcceptPWAInstallConfirmationForTesting();

  // Dynamically set the installurl and manifestid attributes.
  const GURL install_url =
      https_server()->GetURL("/web_apps/install_url/install_url.html");
  ASSERT_TRUE(SetButtonInstallUrl(install_url));
  const GURL manifest_id =
      https_server()->GetURL("/web_apps/install_url/index.html");
  ASSERT_TRUE(SetButtonManifestId(manifest_id));

  // Click the install element and wait for the app to open.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ClickElementWithId(kInstallElementId);
  Browser* web_app_browser = browser_created_observer.Wait();

  // Verify promptaction event was fired.
  WaitForPromptActionEvent(kInstallElementId);

  // Verify the app launched.
  ASSERT_TRUE(AppBrowserController::IsWebApp(web_app_browser));
  const WebAppBrowserController* app_controller =
      WebAppBrowserController::From(web_app_browser);
  EXPECT_EQ(app_controller->GetTitle(), u"Simple web app");

  // The registrar should now have one app installed.
  EXPECT_EQ(provider().registrar_unsafe().GetAppIds().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(InstallElementBrowserTest, InstallWithUrl_UserDenies) {
  // Navigate to a page with <install> elements.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL("/web_apps/install_element/index.html")));

  // Simulate the user declining the install prompt.
  SetPermissionResponse();
  auto auto_decline_pwa_install_confirmation =
      SetAutoDeclinePWAInstallConfirmationForTesting();

  // Dynamically set the installurl attribute.
  // Since we're installing by URL only, the manifest must contain an id.
  const GURL install_url =
      https_server()->GetURL("/web_apps/custom_id/install_url.html");
  ASSERT_TRUE(SetButtonInstallUrl(install_url));

  // Click the install element.
  ClickElementWithId(kInstallElementId);

  // Verify promptdismiss event was fired.
  WaitForDismissEvent(kInstallElementId);

  // The registrar should still have zero apps installed.
  EXPECT_EQ(provider().registrar_unsafe().GetAppIds().size(), 0u);
}

}  // namespace web_app
