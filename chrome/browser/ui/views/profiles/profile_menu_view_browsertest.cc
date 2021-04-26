// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
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
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
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
    return identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  }

  // signin::IdentityManager::Observer overrides:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override {
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
  profile_manager->RegisterTestingProfile(std::move(profile), true);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  return profile_ptr;
}

}  // namespace

class ProfileMenuViewTestBase {
 public:
 protected:
  void OpenProfileMenu(Browser* browser, bool use_mouse = true) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(
        target_browser_ ? target_browser_ : browser);

    // Click the avatar button to open the menu.
    views::View* avatar_button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    ASSERT_TRUE(avatar_button);
    if (use_mouse) {
      Click(avatar_button);
    } else {
      avatar_button->RequestFocus();
      avatar_button->OnKeyPressed(
          ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));
      avatar_button->OnKeyReleased(
          ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SPACE, ui::EF_NONE));
    }

    ASSERT_TRUE(profile_menu_view());
    profile_menu_view()->set_close_on_deactivate(false);

#if defined(OS_MAC)
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

  ProfileMenuViewBase* profile_menu_view() {
    return static_cast<ProfileMenuViewBase*>(
        ProfileMenuViewBase::GetBubbleForTesting());
  }

  void SetTargetBrowser(Browser* browser) { target_browser_ = browser; }

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

 private:
  Browser* target_browser_ = nullptr;
};

class ProfileMenuViewExtensionsTest : public ProfileMenuViewTestBase,
                                      public extensions::ExtensionBrowserTest {
};

// Make sure nothing bad happens when the browser theme changes while the
// ProfileMenuView is visible. Regression test for crbug.com/737470
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, ThemeChanged) {
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu(browser()));

  // The theme change destroys the avatar button. Make sure the profile chooser
  // widget doesn't try to reference a stale observer during its shutdown.
  test::ThemeServiceChangedWaiter waiter(
      ThemeServiceFactory::GetForProfile(profile()));
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  waiter.WaitForThemeChanged();

  EXPECT_TRUE(ProfileMenuView::IsShowing());
  profile_menu_view()->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Profile chooser view should close when a tab is added.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, CloseBubbleOnTadAdded) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu(browser()));
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

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu(browser()));
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

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu(browser()));
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

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu(browser()));
  tab_strip->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuView::IsShowing());
}

// Test that sets up a primary account (without sync) and simulates a click on
// the signout button.
class ProfileMenuViewSignoutTest : public ProfileMenuViewTestBase,
                                   public InProcessBrowserTest {
 public:
  ProfileMenuViewSignoutTest() = default;

  CoreAccountId account_id() const { return account_id_; }

  bool Signout() {
    OpenProfileMenu(browser());
    if (HasFatalFailure())
      return false;
    static_cast<ProfileMenuView*>(profile_menu_view())
        ->OnSignoutButtonClicked();
    return true;
  }

  void SetUpOnMainThread() override {
    // Add an account (no sync).
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    account_id_ =
        signin::MakeAccountAvailable(identity_manager, "foo@example.com")
            .account_id;
    ASSERT_TRUE(identity_manager->HasAccountWithRefreshToken(account_id_));
  }

 private:
  CoreAccountId account_id_;
};

// Checks that signout opens a new logout tab.
IN_PROC_BROWSER_TEST_F(ProfileMenuViewSignoutTest, OpenLogoutTab) {
  // Start from a page that is not the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com"));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ(0, tab_strip->active_index());
  EXPECT_NE(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetActiveWebContents()->GetURL());

  // Signout creates a new tab.
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  ASSERT_TRUE(Signout());
  tab_waiter.Wait();
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(1, tab_strip->active_index());
  content::WebContents* logout_page = tab_strip->GetActiveWebContents();
  EXPECT_EQ(GaiaUrls::GetInstance()->service_logout_url(),
            logout_page->GetURL());
}

// Checks that the NTP is navigated to the logout URL, instead of creating
// another tab.
// Flaky on Linux, at least. crbug.com/1116606
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_SignoutFromNTP DISABLED_SignoutFromNTP
#else
#define MAYBE_SignoutFromNTP SignoutFromNTP
#endif
IN_PROC_BROWSER_TEST_F(ProfileMenuViewSignoutTest, MAYBE_SignoutFromNTP) {
  // Start from the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ(0, tab_strip->active_index());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetActiveWebContents()->GetURL());

  // Signout navigates the current tab.
  ASSERT_TRUE(Signout());
  EXPECT_EQ(1, tab_strip->count());
  content::WebContents* logout_page = tab_strip->GetActiveWebContents();
  EXPECT_EQ(GaiaUrls::GetInstance()->service_logout_url(),
            logout_page->GetURL());
}

// Signout test that handles logout requests. The parameter indicates whether
// an error page is generated for the logout request.
class ProfileMenuViewSignoutTestWithNetwork
    : public ProfileMenuViewSignoutTest,
      public testing::WithParamInterface<bool> {
 public:
  ProfileMenuViewSignoutTestWithNetwork()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ProfileMenuViewSignoutTestWithNetwork::HandleSignoutURL,
        has_network_error()));
  }

  // Simple wrapper around GetParam(), with a better name.
  bool has_network_error() const { return GetParam(); }

  // Handles logout requests, either with success or an error page.
  static std::unique_ptr<net::test_server::HttpResponse> HandleSignoutURL(
      bool has_network_error,
      const net::test_server::HttpRequest& request) {
    if (!net::test_server::ShouldHandle(
            request, GaiaUrls::GetInstance()->service_logout_url().path())) {
      return nullptr;
    }

    if (has_network_error) {
      // Return invalid response, triggers an error page.
      return std::make_unique<net::test_server::RawHttpResponse>("", "");
    } else {
      // Return a dummy successful response.
      return std::make_unique<net::test_server::BasicHttpResponse>();
    }
  }

  // Returns whether the web contents is displaying an error page.
  static bool IsErrorPage(content::WebContents* web_contents) {
    return web_contents->GetController()
               .GetLastCommittedEntry()
               ->GetPageType() == content::PAGE_TYPE_ERROR;
  }

  // InProcessBrowserTest:
  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    ProfileMenuViewSignoutTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ProfileMenuViewSignoutTest::SetUpCommandLine(command_line);
    const GURL& base_url = https_server_.base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
  }

  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();
    ProfileMenuViewSignoutTest::SetUpOnMainThread();
  }

 private:
  net::EmbeddedTestServer https_server_;
};

// Tests that the local signout is performed (tokens are deleted) only if the
// logout tab failed to load.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewSignoutTestWithNetwork, Signout) {
  // The test starts from about://blank, which causes the logout to happen in
  // the current tab.
  ASSERT_TRUE(Signout());
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* logout_page = tab_strip->GetActiveWebContents();
  EXPECT_EQ(GaiaUrls::GetInstance()->service_logout_url(),
            logout_page->GetURL());

  // Wait until navigation is finished.
  content::TestNavigationObserver navigation_observer(logout_page);
  navigation_observer.Wait();

  EXPECT_EQ(IsErrorPage(logout_page), has_network_error());
  // If there is a load error, the token is deleted locally, otherwise nothing
  // happens because we rely on Gaia to perform the signout.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(identity_manager->HasAccountWithRefreshToken(account_id()),
            !has_network_error());
}

INSTANTIATE_TEST_SUITE_P(NetworkOnOrOff,
                         ProfileMenuViewSignoutTestWithNetwork,
                         ::testing::Bool());

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
class ProfileMenuClickTestBase : public SyncTest,
                                 public ProfileMenuViewTestBase {
 public:
  ProfileMenuClickTestBase() : SyncTest(SINGLE_CLIENT) {}
  ~ProfileMenuClickTestBase() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_subscription_ =
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

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  syncer::ProfileSyncService* sync_service() {
    return ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
        browser()->profile());
  }

  ProfileSyncServiceHarness* sync_harness() { return sync_harness_.get(); }

 protected:
  void AdvanceFocus(int count) {
    for (int i = 0; i < count; i++)
      profile_menu_view()->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
  }

  views::View* GetFocusedItem() {
    return profile_menu_view()->GetFocusManager()->GetFocusedView();
  }

  base::CallbackListSubscription test_signin_client_subscription_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<ProfileSyncServiceHarness> sync_harness_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuClickTestBase);
};

class ProfileMenuClickTest : public ProfileMenuClickTestBase,
                             public testing::WithParamInterface<size_t> {
 public:
  ProfileMenuClickTest() = default;
  ~ProfileMenuClickTest() override = default;

  virtual ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex(
      size_t index) = 0;

  // This should be called in the test body.
  void RunTest() {
    ASSERT_NO_FATAL_FAILURE(OpenProfileMenu(browser()));
    // These tests don't care about performing the actual menu actions, only
    // about the histogram recorded.
    ASSERT_TRUE(profile_menu_view());
    profile_menu_view()->set_perform_menu_actions_for_testing(false);
    AdvanceFocus(/*count=*/GetParam() + 1);
    ASSERT_TRUE(GetFocusedItem());
    Click(GetFocusedItem());
    LOG(INFO) << "Clicked item at index " << GetParam();
    base::RunLoop().RunUntilIdle();

    histogram_tester_.ExpectUniqueSample(
        "Profile.Menu.ClickedActionableItem",
        GetExpectedActionableItemAtIndex(GetParam()), /*count=*/1);
  }

 private:
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

// Flaky (crbug.com/1025493).
#if defined(OS_LINUX)
#define MAYBE_ProfileMenuClickTest_SingleProfileWithCustomName \
  DISABLED_ProfileMenuClickTest_SingleProfileWithCustomName
#else
#define MAYBE_ProfileMenuClickTest_SingleProfileWithCustomName \
  ProfileMenuClickTest_SingleProfileWithCustomName
#endif
PROFILE_MENU_CLICK_TEST(
    kActionableItems_SingleProfileWithCustomName,
    MAYBE_ProfileMenuClickTest_SingleProfileWithCustomName) {
  profiles::UpdateProfileName(browser()->profile(), u"Custom name");
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
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
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
    ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SyncEnabled,
                        ProfileMenuClickTest_SyncEnabled) {
  ASSERT_TRUE(sync_harness()->SetupSync());
  // Check that the sync setup was successful.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem kActionableItems_SyncError[] = {
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
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
    ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SyncError,
                        ProfileMenuClickTest_SyncError) {
  ASSERT_TRUE(sync_harness()->SignInPrimaryAccount());
  // Check that the setup was successful.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_FALSE(sync_service()->IsSyncFeatureEnabled());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem kActionableItems_SyncPaused[] = {
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kPasswordsButton,
    ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
    ProfileMenuViewBase::ActionableItem::kAddressesButton,
    ProfileMenuViewBase::ActionableItem::kSyncErrorButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SyncPaused,
                        ProfileMenuClickTest_SyncPaused) {
  ASSERT_TRUE(sync_harness()->SetupSync());
  sync_harness()->EnterSyncPausedStateForPrimaryAccount();
  // Check that the setup was successful.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_EQ(syncer::SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SigninDisallowed[] = {
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SigninDisallowed,
                        ProfileMenuClickTest_SigninDisallowed) {
  // Check that the setup was successful.
  ASSERT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed));

  RunTest();
}

IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_SigninDisallowed,
                       PRE_ProfileMenuClickTest_SigninDisallowed) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSigninAllowedOnNextStartup, false);
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_WithUnconsentedPrimaryAccount[] = {
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
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
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

// Flaky (crbug.com/1021930).
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_ProfileMenuClickTest_WithUnconsentedPrimaryAccount \
  DISABLED_ProfileMenuClickTest_WithUnconsentedPrimaryAccount
#else
#define MAYBE_ProfileMenuClickTest_WithUnconsentedPrimaryAccount \
  ProfileMenuClickTest_WithUnconsentedPrimaryAccount
#endif
PROFILE_MENU_CLICK_TEST(
    kActionableItems_WithUnconsentedPrimaryAccount,
    MAYBE_ProfileMenuClickTest_WithUnconsentedPrimaryAccount) {
  secondary_account_helper::SignInSecondaryAccount(
      browser()->profile(), &test_url_loader_factory_, "user@example.com");
  UnconsentedPrimaryAccountChecker(identity_manager()).Wait();
  // Check that the setup was successful.
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

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

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem kActionableItems_GuestProfile[] =
    {ProfileMenuViewBase::ActionableItem::kExitProfileButton,
     // The first button is added again to finish the cycle and test that
     // there are no other buttons at the end.
     // Note that the test does not rely on the specific order of running test
     // instances, but considers the relative order of the actionable items in
     // this array. So for the last item, it does N+1 steps through the menu (N
     // being the number of items in the menu) and checks if the last item in
     // this array triggers the same action as the first one.
     ProfileMenuViewBase::ActionableItem::kExitProfileButton};

// TODO(https://crbug.com/1125474): Revert to using PROFILE_MENU_CLICK_TEST when
// non-ephemeral Guest profiles are removed.
class GuestProfileMenuClickTest : public ProfileMenuClickTest {
 public:
  GuestProfileMenuClickTest() {
    TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
        scoped_feature_list_, false);
  }

  ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex(
      size_t index) override {
    return kActionableItems_GuestProfile[index];
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(GuestProfileMenuClickTest);
};

IN_PROC_BROWSER_TEST_P(GuestProfileMenuClickTest,
                       ProfileMenuClickTest_GuestProfile) {
  Browser* browser = CreateGuestBrowser();
  ASSERT_TRUE(browser);

  // Open a second guest browser window, so the ExitProfileButton is shown.
  SetTargetBrowser(CreateGuestBrowser());

  RunTest();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GuestProfileMenuClickTest,
    ::testing::Range(size_t(0), base::size(kActionableItems_GuestProfile)));

// TODO(https://crbug.com/1125474): Remove OS_CHROMEOS and enable for Lacros
// when supported.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_EphemeralGuestProfile[] = {
        ProfileMenuViewBase::ActionableItem::kExitProfileButton};

class EphemeralGuestProfileMenuClickTest : public ProfileMenuClickTest {
 public:
  EphemeralGuestProfileMenuClickTest() {
    EXPECT_TRUE(TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
        scoped_feature_list_, true));
  }

  ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex(
      size_t index) override {
    return kActionableItems_EphemeralGuestProfile[index];
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(EphemeralGuestProfileMenuClickTest);
};

IN_PROC_BROWSER_TEST_P(EphemeralGuestProfileMenuClickTest,
                       ProfileMenuClickTest_GuestProfile) {
  Browser* browser = CreateGuestBrowser();
  ASSERT_TRUE(browser);

  // Open a second guest browser window, so the ExitProfileButton is shown.
  SetTargetBrowser(CreateGuestBrowser());

  RunTest();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EphemeralGuestProfileMenuClickTest,
    ::testing::Range(size_t(0),
                     base::size(kActionableItems_EphemeralGuestProfile)));
#endif  // defined(OS_WIN) || defined(OS_MAC) || (defined(OS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

class ProfileMenuClickKeyAcceleratorTest : public ProfileMenuClickTestBase {
 public:
  ProfileMenuClickKeyAcceleratorTest() = default;
  ~ProfileMenuClickKeyAcceleratorTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileMenuClickKeyAcceleratorTest);
};

IN_PROC_BROWSER_TEST_F(ProfileMenuClickKeyAcceleratorTest, FocusOtherProfile) {
  // Add an additional profiles.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  CreateTestingProfile(profile_manager->GenerateNextProfileDirectoryPath());

  // Open the menu using the keyboard.
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu(browser(), /*use_mouse=*/false));

  // This test doesn't care about performing the actual menu actions, only
  // about the histogram recorded.
  ASSERT_TRUE(profile_menu_view());
  profile_menu_view()->set_perform_menu_actions_for_testing(false);

  // The first other profile menu should be focused when the menu is opened
  // via a key event.
  views::View* focused_view = GetFocusedItem();
  ASSERT_TRUE(focused_view);
  focused_view->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE));
  focused_view->OnKeyReleased(
      ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_RETURN, ui::EF_NONE));
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Profile.Menu.ClickedActionableItem",
      ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
      /*expected_count=*/1);
}
