// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/desks/desks_util.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"
#endif

namespace ash {
namespace {

using InteractiveMixinBasedBrowserTest =
    InteractiveBrowserTestT<MixinBasedInProcessBrowserTest>;

class QuickSettingsIntegrationTest : public InteractiveMixinBasedBrowserTest {
 public:
  QuickSettingsIntegrationTest() {
    feature_list_.InitAndEnableFeature(features::kQsRevamp);

    // This test suite does not require a browser window.
    set_launch_browser_for_testing(nullptr);

    // Use the per-display root window as the context for all widgets.
    views::ElementTrackerViews::SetContextOverrideCallback(
        base::BindRepeating([](views::Widget* widget) {
          return ui::ElementContext(widget->GetNativeWindow()->GetRootWindow());
        }));
  }

  // InteractiveMixinBasedBrowserTest:
  void SetUpOnMainThread() override {
    InteractiveMixinBasedBrowserTest::SetUpOnMainThread();

    // Ensure the OS Settings system web app (SWA) is installed.
    Profile* profile = ProfileManager::GetActiveUserProfile();
    CHECK(profile);
    SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();
  }

  void TearDownOnMainThread() override {
    // Clean up any browsers we opened (including the SWA browser) otherwise
    // the test may hang on shutdown.
    // TODO(b/292067979): Find a better way to work around this issue.
    for (Browser* browser : *BrowserList::GetInstance()) {
      CloseBrowserSynchronously(browser);
    }
    InteractiveMixinBasedBrowserTest::TearDownOnMainThread();
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  // This test runs on linux-chromeos in interactive_ui_tests and on a DUT in
  // chromeos_integration_tests.
  ChromeOSIntegrationTestMixin chromeos_integration_test_mixin_{&mixin_host_};
#endif
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(QuickSettingsIntegrationTest, OpenOsSettings) {
  // Kombucha requires a context widget to synthesize clicks.
  views::Widget* status_area_widget =
      Shell::GetPrimaryRootWindowController()->shelf()->GetStatusAreaWidget();
  SetContextWidget(status_area_widget);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsElementId);

  RunTestSequence(
      Log("Opening quick settings bubble"),
      PressButton(kUnifiedSystemTrayElementId),
      WaitForShow(kQuickSettingsViewElementId),

      Log("Clicking settings button"),
      InstrumentNextTab(kOsSettingsElementId, AnyBrowser()),
      PressButton(kQuickSettingsSettingsButtonElementId),
      WaitForShow(kOsSettingsElementId),

      Log("Verifying that OS Settings loads"),
      WaitForWebContentsReady(kOsSettingsElementId,
                              GURL(chrome::kChromeUIOSSettingsURL)));
}

}  // namespace
}  // namespace ash
