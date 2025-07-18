// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_request_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

namespace {

// A dummy WebContentsDelegate which tracks whether CloseContents() has been
// called. It refuses the actual close but keeps track of whether the renderer
// requested it.
class CloseTrackingDelegate : public content::WebContentsDelegate {
 public:
  CloseTrackingDelegate() = default;

  CloseTrackingDelegate(const CloseTrackingDelegate&) = delete;
  CloseTrackingDelegate& operator=(const CloseTrackingDelegate&) = delete;

  bool close_contents_called() const { return close_contents_called_; }

  void CloseContents(content::WebContents* source) override {
    close_contents_called_ = true;
  }

 private:
  bool close_contents_called_ = false;
};

}  // namespace

class IsolatedWebAppsCloseWindowBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  IsolatedWebAppsCloseWindowBrowserTest() = default;

  void TearDownOnMainThread() override {
    // After tests finish, the browser pointer is released.
    browser_ = nullptr;

    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return browser()->profile(); }

  AssertionResult InstallAndOpenIWA(bool has_wm_permission_policy) {
    url_info_ = InstallIWA(has_wm_permission_policy);
    browser_ =
        web_app::LaunchWebAppBrowserAndWait(profile(), url_info().app_id());
    return browser_ ? AssertionSuccess()
                    : AssertionFailure() << "Failed to open browser";
  }

  web_app::IsolatedWebAppUrlInfo InstallIWA(bool has_wm_permission_policy) {
    auto manifest_builder = web_app::ManifestBuilder().SetName("App name");

    if (has_wm_permission_policy) {
      manifest_builder.AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kWindowManagement,
          /*self=*/true, /*origins=*/{});
    }
    return web_app::IsolatedWebAppBuilder(manifest_builder)
        .AddHtml("/page1", "<!DOCTYPE html><body>page1</body>")
        .BuildBundle()
        ->InstallChecked(profile());
  }

  // window.close is allowed if the window was opened by DOM OR the back/forward
  // list has only one element. Navigate so the second condition is false.
  void FillNavigationHistory() {
    ASSERT_NE(ui_test_utils::NavigateToURL(
                  browser_, url_info().origin().GetURL().Resolve("/page1")),
              nullptr);
  }

  bool AttemptCloseFromJavaScript() {
    content::WebContents* web_contents = GetActiveWebContents();
    // The old functional delegate is saved and put back, so that WebContents
    // can be used after normally, like for navigations.
    CloseTrackingDelegate close_tracking_delegate;
    content::WebContentsDelegate* old_delegate = web_contents->GetDelegate();
    web_contents->SetDelegate(&close_tracking_delegate);

    const char kCloseWindowScript[] =
        // Close the window.
        "window.close();"
        // Report back after an event loop iteration; the close IPC isn't sent
        // immediately.
        "new Promise(resolve => setTimeout(() => {"
        "  resolve(0);"
        "}));";
    EXPECT_EQ(0, EvalJs(web_contents, kCloseWindowScript).ExtractInt());

    web_contents->SetDelegate(old_delegate);
    return close_tracking_delegate.close_contents_called();
  }

  void GrantWindowManagementPermission() {
    auto* manager = permissions::PermissionRequestManager::FromWebContents(
        GetActiveWebContents());
    manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);

    // This call automatically requests permission.
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), "getScreenDetails()"));

    blink::mojom::PermissionStatus permission_status =
        profile()
            ->GetPermissionController()
            ->GetPermissionStatusForCurrentDocument(
                content::PermissionDescriptorUtil::
                    CreatePermissionDescriptorForPermissionType(
                        blink::PermissionType::WINDOW_MANAGEMENT),
                GetActiveWebContents()->GetPrimaryMainFrame());

    ASSERT_EQ(permission_status, blink::mojom::PermissionStatus::GRANTED);
  }

  // Emulates user changing permission in the browser settings.
  void SetContentSetting(ContentSetting content_setting) {
    HostContentSettingsMap* content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    content_settings_map->SetContentSettingDefaultScope(
        GetActiveWebContents()->GetLastCommittedURL(),
        GetActiveWebContents()->GetLastCommittedURL(),
        ContentSettingsType::WINDOW_MANAGEMENT, content_setting);
  }

  content::WebContents* GetActiveWebContents() {
    content::WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(url_info().app_id(),
              *web_app::WebAppTabHelper::GetAppId(web_contents));

    return web_contents;
  }

  web_app::IsolatedWebAppUrlInfo url_info() {
    CHECK(url_info_);
    return url_info_.value();
  }

  raw_ptr<Browser> browser_;
  // Always present. optional is needed because it is not initialized in
  // test constructor and web_app::IsolatedWebAppUrlInfo has no default
  // constructor.
  std::optional<web_app::IsolatedWebAppUrlInfo> url_info_;
};

// window.close() works on a normal window that has no navigation history.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppsCloseWindowBrowserTest,
                       NoNavigationHistory) {
  ASSERT_TRUE(InstallAndOpenIWA(/*has_wm_permission_policy=*/true));
  EXPECT_TRUE(AttemptCloseFromJavaScript());
}

// window.close() does not work on a normal window that has navigation history.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppsCloseWindowBrowserTest,
                       HasNavigationHistory) {
  ASSERT_TRUE(InstallAndOpenIWA(/*has_wm_permission_policy=*/true));
  FillNavigationHistory();

  // This window was not opened by DOM, so close does not reach the browser
  // process.
  EXPECT_FALSE(AttemptCloseFromJavaScript());
}

// window.close() works in IWA if window-management permission is granted even
// with navigation history.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppsCloseWindowBrowserTest,
                       GrantWMPermission) {
  ASSERT_TRUE(InstallAndOpenIWA(/*has_wm_permission_policy=*/true));
  FillNavigationHistory();
  GrantWindowManagementPermission();
  EXPECT_TRUE(AttemptCloseFromJavaScript());
}

// window.close() does not work if window-management content setting is granted
// but permission-policy is not.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppsCloseWindowBrowserTest,
                       NoPermissionPolicy) {
  ASSERT_TRUE(InstallAndOpenIWA(/*has_wm_permission_policy=*/false));

  // Emulate that a user grants permission via settings.
  SetContentSetting(CONTENT_SETTING_ALLOW);
  FillNavigationHistory();

  EXPECT_FALSE(AttemptCloseFromJavaScript());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppsCloseWindowBrowserTest,
                       PermissionRevoked) {
  ASSERT_TRUE(InstallAndOpenIWA(/*has_wm_permission_policy=*/true));
  FillNavigationHistory();
  GrantWindowManagementPermission();

  // Emulate that a user revokes the permission via settings.
  SetContentSetting(CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(AttemptCloseFromJavaScript());
}
