// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unload_controller.h"

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/prevent_close_test_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

constexpr char kPreventCloseEnabledForCalculator[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": true
  }
])";

constexpr char kCalculatorForceInstalled[] = R"([
  {
    "url": "https://calculator.apps.chrome/",
    "default_launch_container": "window"
  }
])";

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kShouldPreventClose = true;
#else
constexpr bool kShouldPreventClose = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class UnloadControllerPreventCloseTest
    : public VerticalTabsBrowserTestMixin<PreventCloseTestBase>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  UnloadControllerPreventCloseTest() = default;
  ~UnloadControllerPreventCloseTest() override = default;

  TabStripOrientation orientation() const { return GetParam(); }
  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  void SetUpOnMainThread() override {
    VerticalTabsBrowserTestMixin<PreventCloseTestBase>::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
  }
};

IN_PROC_BROWSER_TEST_P(UnloadControllerPreventCloseTest,
                       PreventCloseEnforcedByPolicy) {
  const absl::Cleanup policy_cleanup = [this] {
    SetPolicies(/*web_app_settings=*/"[]", /*web_app_install_force_list=*/"[]");
  };

  InstallPWA(GURL(kCalculatorAppUrl), ash::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(ash::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);

  BrowserWindowInterface* const browser =
      LaunchPWA(ash::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  UnloadController* unload_controller = UnloadController::From(browser);
  EXPECT_EQ(kShouldPreventClose
                ? BrowserWindowInterface::ClosingStatus::kDeniedByPolicy
                : BrowserWindowInterface::ClosingStatus::kPermitted,
            unload_controller->GetBrowserClosingStatus());
}

// Flaky on IS_CHROMEOS. crbug.com/369817361
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable \
  DISABLED_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable
#else
#define MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable \
  PreventCloseEnforcedByPolicyTabbedAppShallBeClosable
#endif
IN_PROC_BROWSER_TEST_P(
    UnloadControllerPreventCloseTest,
    MAYBE_PreventCloseEnforcedByPolicyTabbedAppShallBeClosable) {
  const absl::Cleanup policy_cleanup = [this] {
    SetPolicies(/*web_app_settings=*/"[]", /*web_app_install_force_list=*/"[]");
  };

  InstallPWA(GURL(kCalculatorAppUrl), ash::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(ash::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);

  BrowserWindowInterface* const browser =
      LaunchPWA(ash::kCalculatorAppId, /*launch_in_window=*/false);
  ASSERT_TRUE(browser);

  UnloadController* unload_controller = UnloadController::From(browser);
  EXPECT_EQ(BrowserWindowInterface::ClosingStatus::kPermitted,
            unload_controller->GetBrowserClosingStatus());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UnloadControllerPreventCloseTest,
    testing::Values(TabStripOrientation::kVertical,
                    TabStripOrientation::kHorizontal),
    [](const testing::TestParamInfo<TabStripOrientation>& info) {
      switch (info.param) {
        case TabStripOrientation::kVertical:
          return "Vertical";
        case TabStripOrientation::kHorizontal:
          return "Horizontal";
      }
    });

#if BUILDFLAG(IS_CHROMEOS)

// Browser tests for verifying `UnloadController` behavior for apps when locked
// (and not locked) for OnTask. Only relevant for non-web browser scenarios.
class UnloadControllerWithOnTaskTest : public InProcessBrowserTest {
 protected:
  webapps::AppId InstallMockApp() {
    return web_app::test::InstallDummyWebApp(
        browser()->GetProfile(), /*app_name=*/"Mock app",
        /*app_url=*/GURL("https://www.example.com/"));
  }
};

IN_PROC_BROWSER_TEST_F(UnloadControllerWithOnTaskTest,
                       PreventCloseWhenLockedForOnTask) {
  // Install and launch app.
  webapps::AppId app_id = InstallMockApp();
  BrowserWindowInterface* const app_browser =
      web_app::LaunchWebAppBrowser(browser()->GetProfile(), app_id);
  ash::boca::OnTaskLockedController::From(app_browser)
      ->set_locked_for_on_task(true);

  // Verify tab cannot be closed.
  content::WebContents* const active_web_contents =
      app_browser->GetTabStripModel()->GetWebContentsAt(0);
  UnloadController* unload_controller = UnloadController::From(app_browser);
  EXPECT_FALSE(unload_controller->CanCloseContents(active_web_contents));
}

IN_PROC_BROWSER_TEST_F(UnloadControllerWithOnTaskTest,
                       AllowCloseWhenNotLockedForOnTask) {
  // Install and launch app.
  webapps::AppId app_id = InstallMockApp();
  BrowserWindowInterface* const app_browser =
      web_app::LaunchWebAppBrowser(browser()->GetProfile(), app_id);
  ash::boca::OnTaskLockedController::From(app_browser)
      ->set_locked_for_on_task(false);

  // Verify tab can be closed.
  content::WebContents* const active_web_contents =
      app_browser->GetTabStripModel()->GetWebContentsAt(0);
  UnloadController* unload_controller = UnloadController::From(app_browser);
  EXPECT_TRUE(unload_controller->CanCloseContents(active_web_contents));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

using UnloadControllerBrowserTest = InProcessBrowserTest;

// Regression test for https://crbug.com/532111140. A close request can arrive
// asynchronously (e.g. a delayed ClosePage IPC from the renderer, or a close
// triggered while a tab group is being destroyed) after the WebContents has
// already been detached from the browser's tab strip. There is no tab left for
// the browser to close in that case, so the close must be denied instead of
// being forwarded to the tab strip.
IN_PROC_BROWSER_TEST_F(UnloadControllerBrowserTest,
                       CannotCloseContentsDetachedFromTabStrip) {
  TabStripModel* const tab_strip_model = browser()->GetTabStripModel();
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), /*index=*/1,
                   /*foreground=*/true);
  ASSERT_EQ(2, tab_strip_model->count());

  content::WebContents* const contents = tab_strip_model->GetWebContentsAt(1);
  UnloadController* const unload_controller = UnloadController::From(browser());
  ASSERT_TRUE(unload_controller->CanCloseContents(contents));

  const std::unique_ptr<content::WebContents> detached_contents =
      tab_strip_model->DetachWebContentsAtForInsertion(1);
  ASSERT_EQ(contents, detached_contents.get());
  ASSERT_EQ(TabStripModel::kNoTab,
            tab_strip_model->GetIndexOfWebContents(contents));

  EXPECT_FALSE(unload_controller->CanCloseContents(contents));
}

// As above, but the tab is still alive and simply belongs to a different
// window by the time the close request arrives. The original browser does not
// own it anymore and must not try to close it.
IN_PROC_BROWSER_TEST_F(UnloadControllerBrowserTest,
                       CannotCloseContentsMovedToAnotherWindow) {
  TabStripModel* const tab_strip_model = browser()->GetTabStripModel();
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), /*index=*/1,
                   /*foreground=*/true);
  ASSERT_EQ(2, tab_strip_model->count());

  content::WebContents* const contents = tab_strip_model->GetWebContentsAt(1);
  UnloadController* const unload_controller = UnloadController::From(browser());
  ASSERT_TRUE(unload_controller->CanCloseContents(contents));

  chrome::MoveTabsToNewWindow(browser(), {1});
  ASSERT_EQ(1, tab_strip_model->count());

  const tabs::TabInterface* const tab =
      tabs::TabInterface::MaybeGetFromContents(contents);
  ASSERT_TRUE(tab);
  ASSERT_NE(browser(), tab->GetBrowserWindowInterface());

  EXPECT_FALSE(unload_controller->CanCloseContents(contents));
}
