// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_chooser_view.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"

namespace {

Profile* CreateTestingProfile(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  if (!base::PathExists(path) && !base::CreateDirectory(path))
    NOTREACHED() << "Could not create directory at " << path.MaybeAsASCII();

  Profile* profile =
      Profile::CreateProfile(path, nullptr, Profile::CREATE_MODE_SYNCHRONOUS);
  profile_manager->RegisterTestingProfile(profile, true, false);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  return profile;
}

Profile* CreateTestingProfile(const std::string& profile_name) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII(profile_name);
  return CreateTestingProfile(path);
}

// Turns a normal profile into one that's signed in.
void AddAccountToProfile(Profile* profile, const char* signed_in_email) {
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry_signed_in;
  ASSERT_TRUE(storage.GetProfileAttributesWithPath(profile->GetPath(),
                                                   &entry_signed_in));
  entry_signed_in->SetAuthInfo("12345", base::UTF8ToUTF16(signed_in_email));
  profile->GetPrefs()->SetString(prefs::kGoogleServicesHostedDomain,
                                 "google.com");
}

// Set up the profiles to enable Lock. Takes as parameter a profile that will be
// signed in, and also creates a supervised user (necessary for lock), then
// returns the supervised user profile.
Profile* SetupProfilesForLock(Profile* signed_in) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  constexpr char kEmail[] = "me@google.com";
  AddAccountToProfile(signed_in, kEmail);

  // Create the |supervised| profile, which is supervised by |signed_in|.
  ProfileAttributesEntry* entry_supervised;
  Profile* supervised = CreateTestingProfile("supervised");
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  EXPECT_TRUE(storage.GetProfileAttributesWithPath(supervised->GetPath(),
                                                   &entry_supervised));
  entry_supervised->SetSupervisedUserId(kEmail);
  supervised->GetPrefs()->SetString(prefs::kSupervisedUserId, kEmail);

  // |signed_in| should now be lockable.
  EXPECT_TRUE(profiles::IsLockAvailable(signed_in));
  return supervised;
}

}  // namespace

class ProfileChooserViewExtensionsTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest> {
 public:
  ProfileChooserViewExtensionsTest() {}
  ~ProfileChooserViewExtensionsTest() override {}

  // SupportsTestUi:
  void ShowUi(const std::string& name) override {
    // Bubble dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    constexpr char kSignedIn[] = "SignedIn";
    constexpr char kMultiProfile[] = "MultiProfile";
    constexpr char kGuest[] = "Guest";
    constexpr char kDiceGuest[] = "DiceGuest";
    constexpr char kManageAccountLink[] = "ManageAccountLink";
    constexpr char kSupervisedOwner[] = "SupervisedOwner";
    constexpr char kSupervisedUser[] = "SupervisedUser";

    Browser* target_browser = browser();
    if (name == kSignedIn || name == kManageAccountLink) {
      constexpr char kEmail[] = "verylongemailfortesting@gmail.com";
      AddAccountToProfile(target_browser->profile(), kEmail);
    }
    if (name == kMultiProfile) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());
      CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());
    }
    if (name == kGuest || name == kDiceGuest) {
      content::WindowedNotificationObserver browser_creation_observer(
          chrome::NOTIFICATION_BROWSER_OPENED,
          content::NotificationService::AllSources());
      profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
      browser_creation_observer.Wait();

      Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
          ProfileManager::GetGuestProfilePath());
      EXPECT_TRUE(guest);
      target_browser = chrome::FindAnyBrowser(guest, true);
    }

    Profile* supervised = nullptr;
    if (name == kSupervisedOwner || name == kSupervisedUser) {
      supervised = SetupProfilesForLock(target_browser->profile());
    }
    if (name == kSupervisedUser) {
      profiles::SwitchToProfile(supervised->GetPath(), false,
                                ProfileManager::CreateCallback(),
                                ProfileMetrics::ICON_AVATAR_BUBBLE);
      EXPECT_TRUE(supervised);
      target_browser = chrome::FindAnyBrowser(supervised, true);
    }
    OpenProfileChooserView(target_browser);
  }

 protected:
  void OpenProfileChooserView(Browser* browser) {
    ProfileChooserView::close_on_deactivate_for_testing_ = false;
    OpenProfileChooserViews(browser);

    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(ProfileChooserView::IsShowing());

    // Create this observer before lock is pressed to avoid a race condition.
    window_close_observer_.reset(new content::WindowedNotificationObserver(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser)));
  }

  void OpenProfileChooserViews(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    views::View* button = browser_view->toolbar()->avatar_button();
    DCHECK(button);

    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMousePressed(e);
  }

  AvatarMenu* GetProfileChooserViewAvatarMenu() {
    return ProfileChooserView::profile_bubble_->avatar_menu_.get();
  }

  void ClickProfileChooserViewLockButton() {
    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    ProfileChooserView::profile_bubble_->ButtonPressed(
        ProfileChooserView::profile_bubble_->lock_button_, e);
  }

  // Access the registry that has been prepared with at least one extension.
  extensions::ExtensionRegistry* GetPreparedRegistry(Profile* signed_in) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(signed_in);
    const size_t initial_num_extensions = registry->enabled_extensions().size();
    const extensions::Extension* ext = LoadExtension(
        test_data_dir_.AppendASCII("app"));
    EXPECT_TRUE(ext);
    EXPECT_EQ(initial_num_extensions + 1,
              registry->enabled_extensions().size());
    EXPECT_EQ(0U, registry->blocked_extensions().size());
    return registry;
  }

  content::WindowedNotificationObserver* window_close_observer() {
    return window_close_observer_.get();
  }

  ProfileChooserView* current_profile_bubble() {
    return ProfileChooserView::profile_bubble_;
  }

  views::View* signin_current_profile_button() {
    return ProfileChooserView::profile_bubble_->signin_current_profile_button_;
  }

  int GetDiceSigninPromoShowCount() {
    return current_profile_bubble()->GetDiceSigninPromoShowCount();
  }

 private:
  std::unique_ptr<content::WindowedNotificationObserver> window_close_observer_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserViewExtensionsTest);
};

#if defined(OS_WIN)
#define MAYBE_SigninButtonHasFocus DISABLED_SigninButtonHasFocus
#else
#define MAYBE_SigninButtonHasFocus SigninButtonHasFocus
#endif
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       MAYBE_SigninButtonHasFocus) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));

  EXPECT_TRUE(signin_current_profile_button()->HasFocus());
}

// Make sure nothing bad happens when the browser theme changes while the
// ProfileChooserView is visible. Regression test for crbug.com/737470
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, ThemeChanged) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));

  // The theme change destroys the avatar button. Make sure the profile chooser
  // widget doesn't try to reference a stale observer during its shutdown.
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile())));
  theme_change_observer.Wait();

  EXPECT_TRUE(ProfileChooserView::IsShowing());
  current_profile_bubble()->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileChooserView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, ViewProfileUMA) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  base::HistogramTester histograms;
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kProfileAvatarTutorialShown, 0);

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, LockProfile) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  SetupProfilesForLock(browser()->profile());
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  AvatarMenu* menu = GetProfileChooserViewAvatarMenu();
  EXPECT_FALSE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  ClickProfileChooserViewLockButton();
  EXPECT_TRUE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  window_close_observer()->Wait();
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Wait until the user manager is shown.
  runner->Run();

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       LockProfileBlockExtensions) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  // Make sure we have at least one enabled extension.
  extensions::ExtensionRegistry* registry =
      GetPreparedRegistry(browser()->profile());

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  SetupProfilesForLock(browser()->profile());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  ClickProfileChooserViewLockButton();
  window_close_observer()->Wait();

  // Wait until the user manager is shown.
  runner->Run();

  // Assert that the ExtensionService is blocked.
  ASSERT_EQ(1U, registry->blocked_extensions().size());

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       LockProfileNoBlockOtherProfileExtensions) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  // Make sure we have at least one enabled extension.
  extensions::ExtensionRegistry* registry =
      GetPreparedRegistry(browser()->profile());
  const size_t total_enabled_extensions = registry->enabled_extensions().size();

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  // Create a different profile and then lock it.
  Profile* signed_in = CreateTestingProfile("signed_in");
  SetupProfilesForLock(signed_in);
  extensions::ExtensionSystem::Get(signed_in)->InitForRegularProfile(
      true /* extensions_enabled */);
  Browser* browser_to_lock = CreateBrowser(signed_in);
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser_to_lock));
  ClickProfileChooserViewLockButton();
  window_close_observer()->Wait();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Wait until the user manager is shown.
  runner->Run();

  // Assert that the first profile's extensions are not blocked.
  ASSERT_EQ(total_enabled_extensions, registry->enabled_extensions().size());
  ASSERT_EQ(0U, registry->blocked_extensions().size());

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

// Profile chooser view should close when a tab is added.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       CloseBubbleOnTadAdded) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  EXPECT_EQ(1, tab_strip->active_index());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileChooserView::IsShowing());
}

// Profile chooser view should close when active tab is changed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       CloseBubbleOnActiveTabChanged) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  tab_strip->ActivateTabAt(0, false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileChooserView::IsShowing());
}

// Profile chooser view should close when active tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       CloseBubbleOnActiveTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  tab_strip->CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileChooserView::IsShowing());
}

// Profile chooser view should close when the last tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       CloseBubbleOnLastTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  tab_strip->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileChooserView::IsShowing());
}

// Shows a non-signed in profile with no others.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Shows a signed in profile with no others.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, InvokeUi_SignedIn) {
  ShowAndVerifyUi();
}

// Shows the |ProfileChooserView| with three different profiles.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       InvokeUi_MultiProfile) {
  ShowAndVerifyUi();
}

// Shows the |ProfileChooserView| during a Guest browsing session.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, InvokeUi_Guest) {
  ShowAndVerifyUi();
}

// TODO: Flaking test crbug.com/802374
// Shows the |ProfileChooserView| during a Guest browsing session when the DICE
// flag is enabled.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       DISABLED_InvokeUi_DiceGuest) {
  ScopedAccountConsistencyDice scoped_dice;
  ShowAndVerifyUi();
}

// Shows the manage account link, which appears when account consistency is
// enabled for signed-in accounts.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       InvokeUi_ManageAccountLink) {
  ShowAndVerifyUi();
}

// Shows the |ProfileChooserView| from a signed-in account that has a supervised
// user profile attached.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       InvokeUi_SupervisedOwner) {
  ShowAndVerifyUi();
}

// Crashes because account consistency changes:  http://crbug.com/820390
// Shows the |ProfileChooserView| when a supervised user is the active profile.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       DISABLED_InvokeUi_SupervisedUser) {
  ScopedAccountConsistencyDiceFixAuthErrors scoped_account_consistency;
  ShowAndVerifyUi();
}

// Open the profile chooser to increment the Dice sign-in promo show counter
// below the threshold.
// TODO(https://crbug.com/862573): Re-enable when no longer failing when
// is_chrome_branded is true.
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_IncrementDiceSigninPromoShowCounter \
  DISABLED_IncrementDiceSigninPromoShowCounter
#else
#define MAYBE_IncrementDiceSigninPromoShowCounter \
  IncrementDiceSigninPromoShowCounter
#endif
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       MAYBE_IncrementDiceSigninPromoShowCounter) {
  ScopedAccountConsistencyDice scoped_dice;
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, 7);
  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  EXPECT_EQ(GetDiceSigninPromoShowCount(), 8);
}

// The DICE sync illustration is shown only the first 10 times. This test
// ensures that the profile chooser is shown correctly above this threshold.
// TODO(https://crbug.com/862573): Re-enable when no longer failing when
// is_chrome_branded is true.
#if defined(GOOGLE_CHROME_BUILD)
#define MAYBE_DiceSigninPromoWithoutIllustration \
  DISABLED_DiceSigninPromoWithoutIllustration
#else
#define MAYBE_DiceSigninPromoWithoutIllustration \
  DiceSigninPromoWithoutIllustration
#endif
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest,
                       MAYBE_DiceSigninPromoWithoutIllustration) {
  ScopedAccountConsistencyDice scoped_dice;
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, 10);
  ASSERT_NO_FATAL_FAILURE(OpenProfileChooserView(browser()));
  EXPECT_EQ(GetDiceSigninPromoShowCount(), 11);
}

// Verify there is no crash when the chooser is used to display a signed-in
// profile with an empty username.
IN_PROC_BROWSER_TEST_F(ProfileChooserViewExtensionsTest, SignedInNoUsername) {
  AddAccountToProfile(browser()->profile(), "");
  OpenProfileChooserView(browser());
}
