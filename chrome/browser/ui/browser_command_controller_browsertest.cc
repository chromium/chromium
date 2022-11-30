// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_browsertest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ui/ash/window_pin_util.h"
#include "ui/aura/window.h"
#endif

namespace chrome {

class BrowserCommandControllerBrowserTest : public InProcessBrowserTest {
 public:
  BrowserCommandControllerBrowserTest() {}

  BrowserCommandControllerBrowserTest(
      const BrowserCommandControllerBrowserTest&) = delete;
  BrowserCommandControllerBrowserTest& operator=(
      const BrowserCommandControllerBrowserTest&) = delete;

  ~BrowserCommandControllerBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }
};

// Verify that showing a constrained window disables find.
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest, DisableFind) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Showing constrained window should disable find.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto delegate = std::make_unique<MockTabModalConfirmDialogDelegate>(
      web_contents, nullptr);
  MockTabModalConfirmDialogDelegate* delegate_ptr = delegate.get();
  TabModalConfirmDialog::Create(std::move(delegate), web_contents);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Switching to a new (unblocked) tab should reenable it.
  AddBlankTabAndShow(browser());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Switching back to the blocked tab should disable it again.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Closing the constrained window should reenable it.
  delegate_ptr->Cancel();
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       NewAvatarMenuEnabledInGuestMode) {
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  Browser* browser = CreateGuestBrowser();
  EXPECT_TRUE(browser);

  const CommandUpdater* command_updater = browser->command_controller();
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest, LockedFullscreen) {
  CommandUpdaterImpl* command_updater =
      &browser()->command_controller()->command_updater_;
  // IDC_EXIT is always enabled in regular mode so it's a perfect candidate for
  // testing.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_EXIT));
  // Set locked fullscreen mode.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);
  // Update the corresponding command_controller state.
  browser()->command_controller()->LockedFullscreenStateChanged();
  // Update some more states just to make sure the wrong commands don't get
  // enabled.
  browser()->command_controller()->TabStateChanged();
  browser()->command_controller()->FullscreenStateChanged();
  browser()->command_controller()->PrintingStateChanged();
  browser()->command_controller()->ExtensionStateChanged();
  // IDC_EXIT is not enabled in locked fullscreen.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_EXIT));

  constexpr int kAllowlistedIds[] = {IDC_CUT, IDC_COPY, IDC_PASTE};

  // Go through all the command ids and make sure all non-allowlisted commands
  // are disabled.
  for (int id : command_updater->GetAllIds()) {
    if (base::Contains(kAllowlistedIds, id)) {
      continue;
    }
    EXPECT_FALSE(command_updater->IsCommandEnabled(id));
  }

  // Verify the set of allowlisted commands.
  for (int id : kAllowlistedIds) {
    EXPECT_TRUE(command_updater->IsCommandEnabled(id));
  }

  // Exit locked fullscreen mode.
  UnpinWindow(browser()->window()->GetNativeWindow());
  // Update the corresponding command_controller state.
  browser()->command_controller()->LockedFullscreenStateChanged();
  // IDC_EXIT is enabled again.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_EXIT));
}
#endif

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       TestTabRestoreServiceInitialized) {
  // Note: The command should start out as enabled as the default.
  // All the initialization happens before any test code executes,
  // so we can't validate it.

  // The TabRestoreService should get initialized (Loaded)
  // automatically upon launch.
  // Wait for robustness because InProcessBrowserTest::PreRunTestOnMainThread
  // does not flush the task scheduler.
  TabRestoreServiceLoadWaiter waiter(
      TabRestoreServiceFactory::GetForProfile(browser()->profile()));
  waiter.Wait();

  // After initialization, the command should become disabled because there's
  // nothing to restore.
  chrome::BrowserCommandController* commandController =
      browser()->command_controller();
  ASSERT_EQ(false, commandController->IsCommandEnabled(IDC_RESTORE_TAB));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       PRE_TestTabRestoreCommandEnabled) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  content::WebContents* tab_to_close =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab_to_close);
  browser()->tab_strip_model()->CloseSelectedTabs();
  destroyed_watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       TestTabRestoreCommandEnabled) {
  // The TabRestoreService should get initialized (Loaded)
  // automatically upon launch.
  // Wait for robustness because InProcessBrowserTest::PreRunTestOnMainThread
  // does not flush the task scheduler.
  TabRestoreServiceLoadWaiter waiter(
      TabRestoreServiceFactory::GetForProfile(browser()->profile()));
  waiter.Wait();

  // After initialization, the command should remain enabled because there's
  // one tab to restore.
  chrome::BrowserCommandController* commandController =
      browser()->command_controller();
  ASSERT_EQ(true, commandController->IsCommandEnabled(IDC_RESTORE_TAB));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       OpenDisabledForAppBrowser) {
  auto params = Browser::CreateParams::CreateForApp(
      "abcdefghaghpphfffooibmlghaeopach", true /* trusted_source */,
      gfx::Rect(), /* window_bounts */
      browser()->profile(), true /* user_gesture */);
  Browser* browser = Browser::Create(params);

  chrome::BrowserCommandController* commandController =
      browser->command_controller();
  ASSERT_EQ(false, commandController->IsCommandEnabled(IDC_OPEN_FILE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       OpenDisabledForAppPopupBrowser) {
  auto params = Browser::CreateParams::CreateForAppPopup(
      "abcdefghaghpphfffooibmlghaeopach", true /* trusted_source */,
      gfx::Rect(), /* window_bounts */
      browser()->profile(), true /* user_gesture */);
  Browser* browser = Browser::Create(params);

  chrome::BrowserCommandController* commandController =
      browser->command_controller();
  ASSERT_EQ(false, commandController->IsCommandEnabled(IDC_OPEN_FILE));
}

IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       OpenDisabledForDevToolsBrowser) {
  auto params = Browser::CreateParams::CreateForDevTools(browser()->profile());
  Browser* browser = Browser::Create(params);

  chrome::BrowserCommandController* commandController =
      browser->command_controller();
  ASSERT_EQ(false, commandController->IsCommandEnabled(IDC_OPEN_FILE));
}

}  // namespace chrome
