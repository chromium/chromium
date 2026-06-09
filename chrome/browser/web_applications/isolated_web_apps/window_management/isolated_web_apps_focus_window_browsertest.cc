// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/update_user_activation_state_interceptor.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace web_app {

namespace {

content::RenderFrameHost* GetMainFrame(const Browser& browser) {
  content::WebContents* web_contents =
      browser.tab_strip_model()->GetActiveWebContents();
  return web_contents ? web_contents->GetPrimaryMainFrame() : nullptr;
}

bool WaitForMainFrameToFocus(Browser* browser) {
  return base::test::RunUntil([browser]() -> bool {
    content::RenderFrameHost* frame = GetMainFrame(*browser);
    if (!frame) {
      return false;
    }
    content::RenderWidgetHostView* view = frame->GetView();
    return view != nullptr && view->HasFocus();
  });
}

bool IsMainFrameFocused(Browser* browser) {
  content::RenderFrameHost* frame = GetMainFrame(*browser);
  if (!frame) {
    return false;
  }
  return content::EvalJs(frame, "document.hasFocus()",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE)
      .ExtractBool();
}

bool IsWindowActive(Browser* browser) {
  return browser->window()->IsActive();
}

// Struct to make test parameterization clearer than std::tuple.
struct TestParams {
  bool is_wm_permission_declared;  // In the manifest
  bool is_wm_permission_granted;   // In content settings

  std::string ToString() const {
    return base::StringPrintf(
        "Permission%s_%s",
        is_wm_permission_declared ? "Declared" : "Undeclared",
        is_wm_permission_granted ? "Granted" : "Denied");
  }
};

}  // namespace

class IsolatedWebAppFocusBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<TestParams> {
 protected:
  bool IsWindowManagementPermissionDeclared() const {
    return GetParam().is_wm_permission_declared;
  }
  bool IsWindowManagementPermissionGranted() const {
    return GetParam().is_wm_permission_granted;
  }

  bool ShouldFocusWithoutUserActivationWork() const {
    // Focusing without activation should only work if the permission is
    // both declared in the manifest AND granted by the user.
    return IsWindowManagementPermissionDeclared() &&
           IsWindowManagementPermissionGranted();
  }

  IsolatedWebAppUrlInfo InstallIwa(
      bool is_window_management_permission_declared) {
    auto manifest_builder = web_app::ManifestBuilder();
    if (is_window_management_permission_declared) {
      manifest_builder.AddPermissionsPolicy(
          network::mojom::PermissionsPolicyFeature::kWindowManagement,
          /*self=*/true, {});
    }
    return IsolatedWebAppBuilder(manifest_builder)
        .AddHtml("/index.html", "<!DOCTYPE html><body>IWA</body>")
        .AddHtml("/popup.html", "<!DOCTYPE html><body>popup window</body>")
        .BuildBundle()
        ->InstallChecked(profile());
  }

  void SetWindowManagementContentSetting(const GURL& origin) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    ASSERT_TRUE(map);

    ContentSetting content_setting = IsWindowManagementPermissionGranted()
                                         ? CONTENT_SETTING_ALLOW
                                         : CONTENT_SETTING_BLOCK;
    map->SetContentSettingDefaultScope(
        /*primary_url=*/origin, /*secondary_url=*/origin,
        ContentSettingsType::WINDOW_MANAGEMENT, content_setting);
  }

  void ConsumeUserActivation(content::RenderFrameHost* frame) {
    content::UpdateUserActivationStateInterceptor user_activation_interceptor(
        frame);
    user_activation_interceptor.UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
        blink::mojom::UserActivationNotificationType::kTest);
  }

  testing::AssertionResult WindowHasFocus(Browser* browser) {
    if (!WaitForMainFrameToFocus(browser)) {
      return testing::AssertionFailure()
             << "Timed out waiting for browser main frame to focus.";
    }
    if (!IsWindowActive(browser)) {
      return testing::AssertionFailure()
             << "Browser DOM is focused, but OS window is not active.";
    }
    if (!IsMainFrameFocused(browser)) {
      return testing::AssertionFailure()
             << "OS window is active, but Browser DOM is not focused.";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult WindowHasNoFocus(Browser* browser) {
    if (IsWindowActive(browser)) {
      return testing::AssertionFailure()
             << "Expected no focus, but OS window is active.";
    }
    if (IsMainFrameFocused(browser)) {
      return testing::AssertionFailure()
             << "Expected no focus, but Browser DOM is focused.";
    }
    return testing::AssertionSuccess();
  }

  Browser* OpenPopup(content::RenderFrameHost* iwa_frame) {
    ui_test_utils::BrowserCreatedObserver browser_observer;

    EXPECT_TRUE(content::ExecJs(
        iwa_frame, "window.newWinHandle = window.open('/popup.html');"));

    Browser* new_browser = browser_observer.Wait();
    EXPECT_TRUE(new_browser);

    return new_browser;
  }
};

// Tests that window.focus() can shift focus to a new window in an IWA
// without user activation if and only if the window-management permission is
// both declared in the manifest and granted by the user.
IN_PROC_BROWSER_TEST_P(IsolatedWebAppFocusBrowserTest,
                       WindowFocusNoUserActivation) {
#if BUILDFLAG(IS_LINUX)
  // Skip this test if running on Wayland.
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "This test is not supported on Wayland due to the lack of "
                    "Wayland support for window activation";
  }
#endif
  IsolatedWebAppUrlInfo url_info =
      InstallIwa(IsWindowManagementPermissionDeclared());
  Browser* iwa_browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  ASSERT_TRUE(iwa_browser);
  SetWindowManagementContentSetting(url_info.origin().GetURL());

  content::RenderFrameHost* iwa_frame = GetMainFrame(*iwa_browser);
  ASSERT_TRUE(iwa_frame);

  Browser* new_browser = OpenPopup(iwa_frame);

  EXPECT_TRUE(WindowHasFocus(new_browser));
  EXPECT_TRUE(WindowHasNoFocus(iwa_browser));

  // Explicitly activate the original IWA window to shift focus back to it.
  iwa_browser->GetWindow()->Activate();
  EXPECT_TRUE(WindowHasFocus(iwa_browser));
  EXPECT_TRUE(WindowHasNoFocus(new_browser));

  ConsumeUserActivation(iwa_frame);

  // Programmatically focus the new window.
  ASSERT_TRUE(content::ExecJs(iwa_frame, "window.newWinHandle.focus();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  if (ShouldFocusWithoutUserActivationWork()) {
    EXPECT_TRUE(WindowHasFocus(new_browser));
    EXPECT_TRUE(WindowHasNoFocus(iwa_browser));
  } else {
    EXPECT_TRUE(WindowHasFocus(iwa_browser));
    EXPECT_TRUE(WindowHasNoFocus(new_browser));
  }
}

// Tests that an IWA window can self-focus without user activation, if and only
// if the window-management permission is declared and granted.
IN_PROC_BROWSER_TEST_P(IsolatedWebAppFocusBrowserTest,
                       WindowCanFocusItselfWithNoUserActivation) {
#if BUILDFLAG(IS_LINUX)
  // Skip this test if running on Wayland.
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP() << "This test is not supported on Wayland due to the lack of "
                    "Wayland support for window activation";
  }
#endif
  IsolatedWebAppUrlInfo url_info =
      InstallIwa(IsWindowManagementPermissionDeclared());
  Browser* iwa_browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  ASSERT_TRUE(iwa_browser);
  SetWindowManagementContentSetting(url_info.origin().GetURL());

  content::RenderFrameHost* iwa_frame = GetMainFrame(*iwa_browser);
  ASSERT_TRUE(iwa_frame);

  Browser* new_browser = OpenPopup(iwa_frame);

  EXPECT_TRUE(WindowHasFocus(new_browser));
  EXPECT_TRUE(WindowHasNoFocus(iwa_browser));

  ConsumeUserActivation(iwa_frame);

  // The background IWA window tries to focus itself.
  ASSERT_TRUE(content::ExecJs(iwa_frame, "window.focus();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  if (ShouldFocusWithoutUserActivationWork()) {
    EXPECT_TRUE(WindowHasFocus(iwa_browser));
    EXPECT_TRUE(WindowHasNoFocus(new_browser));
  } else {
    EXPECT_TRUE(WindowHasFocus(new_browser));
    EXPECT_TRUE(WindowHasNoFocus(iwa_browser));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppFocusBrowserTest,
    ::testing::Values(TestParams{.is_wm_permission_declared = false,
                                 .is_wm_permission_granted = false},
                      TestParams{.is_wm_permission_declared = true,
                                 .is_wm_permission_granted = false},
                      TestParams{.is_wm_permission_declared = false,
                                 .is_wm_permission_granted = true},
                      TestParams{.is_wm_permission_declared = true,
                                 .is_wm_permission_granted = true}),
    [](const testing::TestParamInfo<TestParams>& info) {
      return info.param.ToString();
    });

}  // namespace web_app
