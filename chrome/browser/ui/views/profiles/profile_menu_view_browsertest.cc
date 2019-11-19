// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/browser/ui/views/profiles/user_manager_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_test.h"

namespace {

class UnconsentedPrimaryAccountChecker
    : public StatusChangeChecker,
      public signin::IdentityManager::Observer {
 public:
  explicit UnconsentedPrimaryAccountChecker(
      signin::IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    identity_manager_->AddObserver(this);
  }
  ~UnconsentedPrimaryAccountChecker() override {
    identity_manager_->RemoveObserver(this);
  }

  // StatusChangeChecker overrides:
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for unconsented primary account";
    return identity_manager_->HasUnconsentedPrimaryAccount();
  }

  // signin::IdentityManager::Observer overrides:
  void OnUnconsentedPrimaryAccountChanged(
      const CoreAccountInfo& unconsented_primary_account_info) override {
    CheckExitCondition();
  }

 private:
  signin::IdentityManager* identity_manager_;
};

Profile* CreateTestingProfile(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  if (!base::PathExists(path) && !base::CreateDirectory(path))
    NOTREACHED() << "Could not create directory at " << path.MaybeAsASCII();

  std::unique_ptr<Profile> profile =
      Profile::CreateProfile(path, nullptr, Profile::CREATE_MODE_SYNCHRONOUS);
  Profile* profile_ptr = profile.get();
  profile_manager->RegisterTestingProfile(std::move(profile), true, false);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  return profile_ptr;
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
  entry_signed_in->SetAuthInfo("12345", base::UTF8ToUTF16(signed_in_email),
                               true);
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

class ProfileMenuViewExtensionsTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest> {
 public:
  ProfileMenuViewExtensionsTest() {}
  ~ProfileMenuViewExtensionsTest() override {}

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
      profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());

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
    OpenProfileMenuView(target_browser);
  }

 protected:
  void OpenProfileMenuView(Browser* browser) {
    ProfileMenuView::close_on_deactivate_for_testing_ = false;
    OpenProfileMenuViews(browser);

    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(ProfileMenuView::IsShowing());
  }

  void OpenProfileMenuViews(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    views::View* button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    DCHECK(button);

    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    button->OnMousePressed(e);
  }

  AvatarMenu* GetProfileMenuViewAvatarMenu() {
    return current_profile_bubble()->avatar_menu_.get();
  }

  void ClickProfileMenuViewLockButton() {
    ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    current_profile_bubble()->ButtonPressed(
        current_profile_bubble()->lock_button_, e);
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

  ProfileMenuView* current_profile_bubble() {
    return static_cast<ProfileMenuView*>(
        ProfileMenuView::GetBubbleForTesting());
  }

  int GetDiceSigninPromoShowCount() {
    return current_profile_bubble()->GetDiceSigninPromoShowCount();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileMenuViewExtensionsTest);
};

// TODO(crbug.com/932818): Remove this class after
// |kAutofillEnableToolbarStatusChip| is cleaned up. Otherwise we need it
// because the toolbar is init-ed before each test is set up. Thus need to
// enable the feature in the general browsertest SetUp().
class ProfileMenuViewExtensionsParamTest
    : public ProfileMenuViewExtensionsTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  ProfileMenuViewExtensionsParamTest()
      : ProfileMenuViewExtensionsTest() {}
  ~ProfileMenuViewExtensionsParamTest() override {}

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          autofill::features::kAutofillEnableToolbarStatusChip);
    }

    ProfileMenuViewExtensionsTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, ClickSigninButton) {
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));

  views::ButtonListener* bubble = current_profile_bubble();
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  base::UserActionTester tester;
  views::FocusManager* focus_manager =
      current_profile_bubble()->GetFocusManager();

  // Advance the focus to the first button in the menu, i.e. the signin button.
  focus_manager->AdvanceFocus(/*reverse=*/false);
  views::Button* signin_button =
      static_cast<views::Button*>(focus_manager->GetFocusedView());
  // Click the button.
  bubble->ButtonPressed(signin_button, event);
  EXPECT_EQ(1, tester.GetActionCount("Signin_Signin_FromAvatarBubbleSignin"));
}

// Make sure nothing bad happens when the browser theme changes while the
// ProfileMenuView is visible. Regression test for crbug.com/737470
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, ThemeChanged) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));

  // The theme change destroys the avatar button. Make sure the profile chooser
  // widget doesn't try to reference a stale observer during its shutdown.
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile())));
  theme_change_observer.Wait();

  EXPECT_TRUE(ProfileMenuView::IsShowing());
  current_profile_bubble()->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, ViewProfileUMA) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  base::HistogramTester histograms;
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kProfileAvatarTutorialShown, 0);

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, LockProfile) {
  ASSERT_TRUE(profiles::IsMultipleProfilesEnabled());

  // Set up the message loop for the user manager.
  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  UserManager::AddOnUserManagerShownCallbackForTesting(runner->QuitClosure());

  SetupProfilesForLock(browser()->profile());
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  AvatarMenu* menu = GetProfileMenuViewAvatarMenu();
  EXPECT_FALSE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  ClickProfileMenuViewLockButton();
  EXPECT_TRUE(menu->GetItemAt(menu->GetActiveProfileIndex()).signin_required);

  if (!BrowserList::GetInstance()->empty())
    ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Wait until the user manager is shown.
  runner->Run();

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
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

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  ClickProfileMenuViewLockButton();

  if (!BrowserList::GetInstance()->empty())
    ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_TRUE(BrowserList::GetInstance()->empty());

  // Wait until the user manager is shown.
  runner->Run();

  // Assert that the ExtensionService is blocked.
  ASSERT_EQ(1U, registry->blocked_extensions().size());

  // We need to hide the User Manager or else the process can't die.
  UserManager::Hide();
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
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

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser_to_lock));
  ClickProfileMenuViewLockButton();

  if (1U != BrowserList::GetInstance()->size())
    ui_test_utils::WaitForBrowserToClose(browser_to_lock);
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
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnTadAdded) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  EXPECT_EQ(1, tab_strip->active_index());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Profile chooser view should close when active tab is changed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnActiveTabChanged) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  tab_strip->ActivateTabAt(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Profile chooser view should close when active tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnActiveTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  tab_strip->CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Profile chooser view should close when the last tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnLastTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  tab_strip->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Shows a non-signed in profile with no others.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

// Shows a signed in profile with no others.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_SignedIn) {
  ShowAndVerifyUi();
}

// Shows the |ProfileMenuView| with three different profiles.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_MultiProfile) {
  ShowAndVerifyUi();
}

// Shows the |ProfileMenuView| during a Guest browsing session.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest, InvokeUi_Guest) {
  ShowAndVerifyUi();
}

// Shows the |ProfileMenuView| during a Guest browsing session when the DICE
// flag is enabled.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest, InvokeUi_DiceGuest) {
  ShowAndVerifyUi();
}

// Shows the manage account link, which appears when account consistency is
// enabled for signed-in accounts.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_ManageAccountLink) {
  ShowAndVerifyUi();
}

// Shows the |ProfileMenuView| from a signed-in account that has a supervised
// user profile attached.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       InvokeUi_SupervisedOwner) {
  ShowAndVerifyUi();
}

// Crashes because account consistency changes:  http://crbug.com/820390
// Shows the |ProfileMenuView| when a supervised user is the active profile.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsParamTest,
                       DISABLED_InvokeUi_SupervisedUser) {
  ShowAndVerifyUi();
}

// Open the profile chooser to increment the Dice sign-in promo show counter
// below the threshold.
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       IncrementDiceSigninPromoShowCounter) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, 7);
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  EXPECT_EQ(GetDiceSigninPromoShowCount(), 8);
}

// The DICE sync illustration is shown only the first 10 times. This test
// ensures that the profile chooser is shown correctly above this threshold.
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       DiceSigninPromoWithoutIllustration) {
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kDiceSigninUserMenuPromoCount, 10);
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuView(browser()));
  EXPECT_EQ(GetDiceSigninPromoShowCount(), 11);
}

// Verify there is no crash when the chooser is used to display a signed-in
// profile with an empty username.
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, SignedInNoUsername) {
  AddAccountToProfile(browser()->profile(), "");
  OpenProfileMenuView(browser());
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileMenuViewExtensionsParamTest,
                         ::testing::Bool());

/*- - - - - - - - - - Profile menu revamp browser tests - - - - - - - - - - -*/

// This class is used to test the existence, the correct order and the call to
// the correct action of the buttons in the profile menu. This is done by
// advancing the focus to each button and simulating a click. It is expected
// that each button records a histogram sample from
// |ProfileMenuViewBase::ActionableItem|.
//
// Subclasses have to implement |GetExpectedActionableItemAtIndex|. The test
// itself should contain the setup and a call to |RunTest|. Example test suite
// instantiation:
//
// class ProfileMenuClickTest_WithPrimaryAccount : public ProfileMenuClickTest {
//   ...
//   ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex(
//      size_t index) override {
//     return ...;
//   }
// };
//
// IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_WithPrimaryAccount,
//  SetupAndRunTest) {
//   ... /* setup primary account */
//   RunTest();
// }
//
// INSTANTIATE_TEST_SUITE_P(
//   ,
//   ProfileMenuClickTest_WithPrimaryAccount,
//   ::testing::Range(0, num_of_actionable_items));
//
class ProfileMenuClickTest : public SyncTest,
                             public testing::WithParamInterface<size_t> {
 public:
  ProfileMenuClickTest() : SyncTest(SINGLE_CLIENT) {
    scoped_feature_list_.InitAndEnableFeature(features::kProfileMenuRevamp);
  }

  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_factory_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    sync_service()->OverrideNetworkForTest(
        fake_server::CreateFakeServerHttpPostProviderFactory(
            GetFakeServer()->AsWeakPtr()));
    sync_harness_ = ProfileSyncServiceHarness::Create(
        browser()->profile(), "user@example.com", "password",
        ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);
  }

  virtual ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex(
      size_t index) = 0;

  // This should be called in the test body.
  void RunTest() {
    ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());
    AdvanceFocus(/*count=*/GetParam() + 1);
    ASSERT_TRUE(GetFocusedItem());
    Click(GetFocusedItem());
    LOG(INFO) << "Clicked item at index " << GetParam();
    base::RunLoop().RunUntilIdle();

    histogram_tester_.ExpectUniqueSample(
        "Profile.Menu.ClickedActionableItem",
        GetExpectedActionableItemAtIndex(GetParam()), /*count=*/1);
  }

  void SetTargetBrowser(Browser* browser) { target_browser_ = browser; }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  syncer::ProfileSyncService* sync_service() {
    return ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
        browser()->profile());
  }

  ProfileSyncServiceHarness* sync_harness() { return sync_harness_.get(); }

 private:
  void OpenProfileMenu() {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(
        target_browser_ ? target_browser_ : browser());

    // Click the avatar button to open the menu.
    views::View* avatar_button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    ASSERT_TRUE(avatar_button);
    Click(avatar_button);

    ASSERT_TRUE(profile_menu_view());
    profile_menu_view()->set_close_on_deactivate(false);

#if defined(OS_MACOSX)
    base::RunLoop().RunUntilIdle();
#else
    // If possible wait until the menu is active.
    views::Widget* menu_widget = profile_menu_view()->GetWidget();
    ASSERT_TRUE(menu_widget);
    if (menu_widget->CanActivate()) {
      views::test::WidgetActivationWaiter(menu_widget, /*active=*/true).Wait();
    } else {
      LOG(ERROR) << "menu_widget can not be activated";
    }
#endif

    LOG(INFO) << "Opening profile menu was successful";
  }

  void AdvanceFocus(int count) {
    for (int i = 0; i < count; i++)
      profile_menu_view()->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
  }

  views::View* GetFocusedItem() {
    return profile_menu_view()->GetFocusManager()->GetFocusedView();
  }

  void Click(views::View* clickable_view) {
    // Simulate a mouse click. Note: Buttons are either fired when pressed or
    // when released, so the corresponding methods need to be called.
    clickable_view->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    clickable_view->OnMouseReleased(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  ProfileMenuViewBase* profile_menu_view() {
    return static_cast<ProfileMenuViewBase*>(
        ProfileMenuViewBase::GetBubbleForTesting());
  }

  secondary_account_helper::ScopedSigninClientFactory
      test_signin_client_factory_;

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  Browser* target_browser_ = nullptr;
  std::unique_ptr<ProfileSyncServiceHarness> sync_harness_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuClickTest);
};

#define PROFILE_MENU_CLICK_TEST(actionable_item_list, test_case_name)     \
  class test_case_name : public ProfileMenuClickTest {                    \
   public:                                                                \
    test_case_name() = default;                                           \
                                                                          \
    ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex( \
        size_t index) override {                                          \
      return actionable_item_list[index];                                 \
    }                                                                     \
                                                                          \
    DISALLOW_COPY_AND_ASSIGN(test_case_name);                             \
  };                                                                      \
                                                                          \
  INSTANTIATE_TEST_SUITE_P(                                               \
      , test_case_name,                                                   \
      ::testing::Range(size_t(0), base::size(actionable_item_list)));     \
                                                                          \
  IN_PROC_BROWSER_TEST_P(test_case_name, test_case_name)

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SingleProfileWithCustomName[] = {
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

// This test is disabled due to being flaky. See https://crbug.com/1025493.
PROFILE_MENU_CLICK_TEST(
    kActionableItems_SingleProfileWithCustomName,
    DISABLED_ProfileMenuClickTest_SingleProfileWithCustomName) {
  profiles::UpdateProfileName(browser()->profile(),
                              base::UTF8ToUTF16("Custom name"));
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_MultipleProfiles[] = {
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
        ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_MultipleProfiles,
                        ProfileMenuClickTest_MultipleProfiles) {
  // Add two additional profiles.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());
  CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());
  // Open a second browser window for the current profile, so the
  // ExitProfileButton is shown.
  SetTargetBrowser(CreateBrowser(browser()->profile()));
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem kActionableItems_SyncEnabled[] = {
    ProfileMenuViewBase::ActionableItem::kPasswordsButton,
    ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
    ProfileMenuViewBase::ActionableItem::kAddressesButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SyncEnabled,
                        ProfileMenuClickTest_SyncEnabled) {
  ASSERT_TRUE(sync_harness()->SetupSync());
  // Check that the sync setup was successful.
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount());
  ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem kActionableItems_SyncError[] = {
    ProfileMenuViewBase::ActionableItem::kPasswordsButton,
    ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
    ProfileMenuViewBase::ActionableItem::kAddressesButton,
    ProfileMenuViewBase::ActionableItem::kSyncErrorButton,
    ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SyncError,
                        ProfileMenuClickTest_SyncError) {
  ASSERT_TRUE(sync_harness()->SignInPrimaryAccount());
  // Check that the setup was successful.
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount());
  ASSERT_FALSE(sync_service()->IsSyncFeatureEnabled());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem kActionableItems_SyncPaused[] = {
    ProfileMenuViewBase::ActionableItem::kPasswordsButton,
    ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
    ProfileMenuViewBase::ActionableItem::kAddressesButton,
    ProfileMenuViewBase::ActionableItem::kSyncErrorButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SyncPaused,
                        ProfileMenuClickTest_SyncPaused) {
  ASSERT_TRUE(sync_harness()->SetupSync());
  sync_harness()->EnterSyncPausedStateForPrimaryAccount();
  // Check that the setup was successful.
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount());
  ASSERT_FALSE(sync_service()->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_PAUSED));

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SigninDisallowed[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SigninDisallowed,
                        ProfileMenuClickTest_SigninDisallowed) {
  // Check that the setup was successful.
  ASSERT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed));

  RunTest();
}

// Setup for the above test.
IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_SigninDisallowed,
                       PRE_ProfileMenuClickTest_SigninDisallowed) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSigninAllowedOnNextStartup, false);
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_WithUnconsentedPrimaryAccount[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kSignoutButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_WithUnconsentedPrimaryAccount,
                        ProfileMenuClickTest_WithUnconsentedPrimaryAccount) {
  secondary_account_helper::SignInSecondaryAccount(
      browser()->profile(), &test_url_loader_factory_, "user@example.com");
  UnconsentedPrimaryAccountChecker(identity_manager()).Wait();
  // Check that the setup was successful.
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount());
  ASSERT_TRUE(identity_manager()->HasUnconsentedPrimaryAccount());

  RunTest();

  if (GetExpectedActionableItemAtIndex(GetParam()) ==
      ProfileMenuViewBase::ActionableItem::kSigninAccountButton) {
    // The sync confirmation dialog was opened after clicking the signin button
    // in the profile menu. It needs to be manually dismissed to not cause any
    // crashes during shutdown.
    EXPECT_TRUE(login_ui_test_utils::DismissSyncConfirmationDialog(
        browser(), base::TimeDelta::FromSeconds(30)));
  }
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem kActionableItems_GuestProfile[] =
    {ProfileMenuViewBase::ActionableItem::kExitProfileButton,
     ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
     ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
     ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
     // The first button is added again to finish the cycle and test that
     // there are no other buttons at the end.
     ProfileMenuViewBase::ActionableItem::kExitProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_GuestProfile,
                        ProfileMenuClickTest_GuestProfile) {
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  ui_test_utils::WaitForBrowserToOpen();
  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  ASSERT_TRUE(guest);
  // Open a second guest browser window, so the ExitProfileButton is shown.
  SetTargetBrowser(CreateIncognitoBrowser(guest));

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_IncognitoProfile[] = {
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kExitProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_IncognitoProfile,
                        ProfileMenuClickTest_IncognitoProfile) {
  SetTargetBrowser(CreateIncognitoBrowser(browser()->profile()));

  RunTest();
}
