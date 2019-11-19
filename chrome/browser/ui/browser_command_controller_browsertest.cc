// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
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
#include "content/public/test/test_utils.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/window.h"
#endif

namespace chrome {

class BrowserCommandControllerBrowserTest : public InProcessBrowserTest {
 public:
  BrowserCommandControllerBrowserTest() {}
  ~BrowserCommandControllerBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCommandControllerBrowserTest);
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

// Note that a Browser's destructor, when the browser's profile is guest, will
// create and execute a BrowsingDataRemover.
// Flakes http://crbug.com/471953
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       DISABLED_NewAvatarMenuEnabledInGuestMode) {
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Create a guest browser nicely. Using CreateProfile() and CreateBrowser()
  // does incomplete initialization that would lead to
  // SystemUrlRequestContextGetter being leaked.
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  ui_test_utils::WaitForBrowserToOpen();
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());

  // Access the browser that was created for the new Guest Profile.
  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  Browser* browser = chrome::FindAnyBrowser(guest, true);
  EXPECT_TRUE(browser);

  // The BrowsingDataRemover needs a loaded TemplateUrlService or else it hangs
  // on to a CallbackList::Subscription forever.
  TemplateURLServiceFactory::GetForProfile(guest)->set_loaded(true);

  const CommandUpdater* command_updater = browser->command_controller();
#if defined(OS_CHROMEOS)
  // Chrome OS uses system tray menu to handle multi-profiles.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
#else
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
#endif
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest, LockedFullscreen) {
  CommandUpdaterImpl* command_updater =
      &browser()->command_controller()->command_updater_;
  // IDC_EXIT is always enabled in regular mode so it's a perfect candidate for
  // testing.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_EXIT));
  // Set locked fullscreen mode.
  browser()->window()->GetNativeWindow()->SetProperty(
      ash::kWindowPinTypeKey, ash::WindowPinType::kTrustedPinned);
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

  constexpr int kWhitelistedIds[] = {IDC_CUT, IDC_COPY, IDC_PASTE};

  // Go through all the command ids and make sure all non-whitelisted commands
  // are disabled.
  for (int id : command_updater->GetAllIds()) {
    if (base::Contains(kWhitelistedIds, id)) {
      continue;
    }
    EXPECT_FALSE(command_updater->IsCommandEnabled(id));
  }

  // Verify the set of whitelisted commands.
  for (int id : kWhitelistedIds) {
    EXPECT_TRUE(command_updater->IsCommandEnabled(id));
  }

  // Exit locked fullscreen mode.
  browser()->window()->GetNativeWindow()->SetProperty(
      ash::kWindowPinTypeKey, ash::WindowPinType::kNone);
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
  TabRestoreServiceLoadWaiter waiter(browser());
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
      browser(), GURL("about:"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
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
  TabRestoreServiceLoadWaiter waiter(browser());
  waiter.Wait();

  // After initialization, the command should remain enabled because there's
  // one tab to restore.
  chrome::BrowserCommandController* commandController =
      browser()->command_controller();
  ASSERT_EQ(true, commandController->IsCommandEnabled(IDC_RESTORE_TAB));
}

}  // namespace chrome
