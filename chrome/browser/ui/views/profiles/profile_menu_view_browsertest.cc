// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
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
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/fake_account_manager_ui_dialog_waiter.h"
#include "chrome/browser/signin/signin_ui_delegate_impl_lacros.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif

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
  raw_ptr<signin::IdentityManager> identity_manager_;
};

Profile* CreateAdditionalProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t starting_number_of_profiles = profile_manager->GetNumberOfProfiles();

  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile* profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  return profile;
}

std::unique_ptr<KeyedService> CreateTestTracker(content::BrowserContext*) {
  return feature_engagement::CreateTestTracker();
}

}  // namespace

class ProfileMenuViewTestBase {
 public:
 protected:
  void OpenProfileMenu() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(target_browser_);

    // Click the avatar button to open the menu.
    views::View* avatar_button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    ASSERT_TRUE(avatar_button);
    Click(avatar_button);

    ASSERT_TRUE(profile_menu_view());
    profile_menu_view()->set_close_on_deactivate(false);

#if BUILDFLAG(IS_MAC)
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
    auto* coordinator = ProfileMenuCoordinator::FromBrowser(target_browser_);
    return coordinator ? coordinator->GetProfileMenuViewBaseForTesting()
                       : nullptr;
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
  raw_ptr<Browser, DanglingUntriaged> target_browser_ = nullptr;
};

class ProfileMenuViewExtensionsTest : public ProfileMenuViewTestBase,
                                      public extensions::ExtensionBrowserTest {
 public:
  ProfileMenuViewExtensionsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        feature_engagement::kIPHProfileSwitchFeature);
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ProfileMenuViewExtensionsTest::RegisterTestTracker));
  }

  // extensions::ExtensionBrowserTest:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    SetTargetBrowser(browser());
  }

 private:
  static void RegisterTestTracker(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestTracker));
  }
  base::CallbackListSubscription subscription_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Make sure nothing bad happens when the browser theme changes while the
// ProfileMenuView is visible. Regression test for crbug.com/737470
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, ThemeChanged) {
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());

  // The theme change destroys the avatar button. Make sure the profile chooser
  // widget doesn't try to reference a stale observer during its shutdown.
  test::ThemeServiceChangedWaiter waiter(
      ThemeServiceFactory::GetForProfile(profile()));
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  waiter.WaitForThemeChanged();

  auto* coordinator = ProfileMenuCoordinator::FromBrowser(browser());
  EXPECT_TRUE(coordinator->IsShowing());
  profile_menu_view()->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(coordinator->IsShowing());
}

// Profile chooser view should close when a tab is added.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, CloseBubbleOnTadAdded) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());
  ASSERT_FALSE(AddTabAtIndex(1, GURL("https://test_url.com"),
                             ui::PageTransition::PAGE_TRANSITION_LINK));
  EXPECT_EQ(1, tab_strip->active_index());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuCoordinator::FromBrowser(browser())->IsShowing());
}

// Profile chooser view should close when active tab is changed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnActiveTabChanged) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_FALSE(AddTabAtIndex(1, GURL("https://test_url.com"),
                             ui::PageTransition::PAGE_TRANSITION_LINK));
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());
  tab_strip->ActivateTabAt(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuCoordinator::FromBrowser(browser())->IsShowing());
}

// Profile chooser view should close when active tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnActiveTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_FALSE(AddTabAtIndex(1, GURL("https://test_url.com"),
                             ui::PageTransition::PAGE_TRANSITION_LINK));
  ASSERT_EQ(2, tab_strip->count());
  ASSERT_EQ(1, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());
  tab_strip->CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ProfileMenuCoordinator::FromBrowser(browser())->IsShowing());
}

// Used to test that the bubble widget is destroyed before the browser.
class BubbleWidgetDestroyedObserver : public views::WidgetObserver {
 public:
  explicit BubbleWidgetDestroyedObserver(views::Widget* bubble_widget) {
    bubble_widget->AddObserver(this);
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    ASSERT_EQ(BrowserList::GetInstance()->size(), 1UL);
  }
};

// Profile chooser view should close when the last tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnLastTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());
  auto widget_destroyed_observer =
      BubbleWidgetDestroyedObserver(profile_menu_view()->GetWidget());
  tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
  base::RunLoop().RunUntilIdle();
}

// Opening the profile menu dismisses any existing IPH.
// Regression test for https://crbug.com/1205901
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, CloseIPH) {
  // Display the IPH.
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
      browser_view->GetFeaturePromoController()));
  EXPECT_TRUE(browser_view->MaybeShowFeaturePromo(
      feature_engagement::kIPHProfileSwitchFeature));
  EXPECT_TRUE(browser_view->IsFeaturePromoActive(
      feature_engagement::kIPHProfileSwitchFeature));

  // Open the menu.
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());

  // Check the IPH is no longer showing.
  EXPECT_FALSE(browser_view->IsFeaturePromoActive(
      feature_engagement::kIPHProfileSwitchFeature));
}

// Test that sets up a primary account (without sync) and simulates a click on
// the signout button.
class ProfileMenuViewSignoutTest : public ProfileMenuViewTestBase,
                                   public InProcessBrowserTest {
 public:
  ProfileMenuViewSignoutTest() = default;

  CoreAccountId account_id() const { return account_id_; }

  bool Signout() {
    OpenProfileMenu();
    if (HasFatalFailure())
      return false;
    static_cast<ProfileMenuView*>(profile_menu_view())
        ->OnSignoutButtonClicked();
    return true;
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(GetProfile());
  }

  Profile* GetProfile() {
    return profile_ ? profile_.get() : browser()->profile();
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void UseSecondaryProfile() {
    // Signout not allowed in the main profile.
    profile_ = CreateAdditionalProfile();
    SetTargetBrowser(CreateBrowser(profile_));
  }
#endif

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetTargetBrowser(browser());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Signout is not allowed on the main profile.
    UseSecondaryProfile();
#endif
    // Add an account (no sync).
    account_id_ =
        signin::MakeAccountAvailable(identity_manager(), "foo@example.com")
            .account_id;
    ASSERT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_));
  }

 private:
  CoreAccountId account_id_;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ProfileMenuViewSignoutTest, Signout) {
  ASSERT_TRUE(Signout());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Checks that signout opens a new logout tab.
IN_PROC_BROWSER_TEST_F(ProfileMenuViewSignoutTest, OpenLogoutTab) {
  // Start from a page that is not the NTP.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));
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
IN_PROC_BROWSER_TEST_F(ProfileMenuViewSignoutTest, SignoutFromNTP) {
  // Start from the NTP.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
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
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Test suite that sets up a primary sync account in an error state and
// simulates a click on the sync error button.
class ProfileMenuViewSyncErrorButtonTest : public ProfileMenuViewTestBase,
                                           public InProcessBrowserTest {
 public:
  ProfileMenuViewSyncErrorButtonTest() = default;

  CoreAccountInfo account_info() const { return account_info_; }

  bool Reauth() {
    OpenProfileMenu();
    if (HasFatalFailure())
      return false;
    // This test does not check that the reauth button is displayed in the menu,
    // but this is tested in ProfileMenuClickTest.
    base::HistogramTester histogram_tester;
    static_cast<ProfileMenuView*>(profile_menu_view())
        ->OnSyncErrorButtonClicked(AvatarSyncErrorType::kAuthError);
    histogram_tester.ExpectUniqueSample(
        "Profile.Menu.ClickedActionableItem",
        ProfileMenuViewBase::ActionableItem::kSyncErrorButton,
        /*expected_bucket_count=*/1);
    return true;
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetTargetBrowser(browser());

    // Add an account.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    account_info_ = signin::MakePrimaryAccountAvailable(
        identity_manager, "foo@example.com", signin::ConsentLevel::kSync);
    signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);
    ASSERT_TRUE(
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info_.account_id));
  }

 private:
  CoreAccountInfo account_info_;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ProfileMenuViewSyncErrorButtonTest, OpenReauthDialog) {
  FakeAccountManagerUI* account_manager_ui = GetFakeAccountManagerUI();
  ASSERT_TRUE(account_manager_ui);
  FakeAccountManagerUIDialogWaiter dialog_waiter(
      account_manager_ui, FakeAccountManagerUIDialogWaiter::Event::kReauth);

  ASSERT_TRUE(Reauth());

  dialog_waiter.Wait();
  EXPECT_TRUE(account_manager_ui->IsDialogShown());
  EXPECT_EQ(1,
            account_manager_ui->show_account_reauthentication_dialog_calls());
}
#else
IN_PROC_BROWSER_TEST_F(ProfileMenuViewSyncErrorButtonTest, OpenReauthTab) {
  // Start from a page that is not the NTP.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  EXPECT_EQ(0, tab_strip->active_index());
  EXPECT_NE(GURL(chrome::kChromeUINewTabURL),
            tab_strip->GetActiveWebContents()->GetURL());

  // Reauth creates a new tab.
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  ASSERT_TRUE(Reauth());
  tab_waiter.Wait();
  EXPECT_EQ(2, tab_strip->count());
  EXPECT_EQ(1, tab_strip->active_index());
  content::WebContents* reauth_page = tab_strip->GetActiveWebContents();
  EXPECT_TRUE(
      base::StartsWith(reauth_page->GetURL().spec(),
                       GaiaUrls::GetInstance()->add_account_url().spec(),
                       base::CompareCase::INSENSITIVE_ASCII));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Test suite that sets up a non-sync account in an error state and simulates a
// click on the sync button.
// This is only relevant on Lacros, as this state does not exist on desktop.
class ProfileMenuViewSigninErrorButtonTest : public ProfileMenuViewTestBase,
                                             public InProcessBrowserTest {
 public:
  class MockSigninUiDelegate
      : public signin_ui_util::SigninUiDelegateImplLacros {
   public:
    MOCK_METHOD(void,
                ShowTurnSyncOnUI,
                (Profile * profile,
                 signin_metrics::AccessPoint access_point,
                 signin_metrics::PromoAction promo_action,
                 signin_metrics::Reason signin_reason,
                 const CoreAccountId& account_id,
                 TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode),
                ());
  };

  ProfileMenuViewSigninErrorButtonTest()
      : delegate_auto_reset_(
            signin_ui_util::SetSigninUiDelegateForTesting(&mock_delegate_)) {}

  CoreAccountInfo account_info() const { return account_info_; }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetTargetBrowser(browser());

    // Add an account, non-syncing and in authentication error.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    account_info_ = signin::MakePrimaryAccountAvailable(
        identity_manager, "foo@example.com", signin::ConsentLevel::kSignin);
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_manager, account_info_.account_id,
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER));
    ASSERT_TRUE(
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info_.account_id));
  }

  void ClickTurnOnSync() {
    OpenProfileMenu();
    // This test does not check that the sync button is displayed in the menu,
    // but this is tested in ProfileMenuClickTest.
    base::HistogramTester histogram_tester;
    static_cast<ProfileMenuView*>(profile_menu_view())
        ->OnSigninAccountButtonClicked(account_info());
    histogram_tester.ExpectUniqueSample(
        "Profile.Menu.ClickedActionableItem",
        ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
        /*expected_bucket_count=*/1);
  }

 protected:
  CoreAccountInfo account_info_;

  testing::StrictMock<MockSigninUiDelegate> mock_delegate_;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset_;
};

IN_PROC_BROWSER_TEST_F(ProfileMenuViewSigninErrorButtonTest, OpenReauthDialog) {
  FakeAccountManagerUI* account_manager_ui = GetFakeAccountManagerUI();
  ASSERT_TRUE(account_manager_ui);
  FakeAccountManagerUIDialogWaiter dialog_waiter(
      account_manager_ui, FakeAccountManagerUIDialogWaiter::Event::kReauth);

  ClickTurnOnSync();

  // Reauth is shown first.
  dialog_waiter.Wait();
  EXPECT_TRUE(account_manager_ui->IsDialogShown());
  EXPECT_EQ(1,
            account_manager_ui->show_account_reauthentication_dialog_calls());

  base::RunLoop loop;
  EXPECT_CALL(
      mock_delegate_,
      ShowTurnSyncOnUI(
          browser()->profile(),
          signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
          signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
          signin_metrics::Reason::kReauthentication, account_info().account_id,
          TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT))
      .WillOnce([&loop]() { loop.Quit(); });

  // Complete reauth.
  account_manager_ui->CloseDialog();

  // Wait until the Sync confirmation is shown.
  loop.Run();
}
#endif

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
                             public ProfileMenuViewTestBase,
                             public testing::WithParamInterface<size_t> {
 public:
  ProfileMenuClickTest() : SyncTest(SINGLE_CLIENT) {}

  ProfileMenuClickTest(const ProfileMenuClickTest&) = delete;
  ProfileMenuClickTest& operator=(const ProfileMenuClickTest&) = delete;

  ~ProfileMenuClickTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

  // SyncTest:
  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();
    SetTargetBrowser(browser());
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void UseSecondaryProfile() {
    profile_ = CreateAdditionalProfile();
    SetTargetBrowser(CreateBrowser(profile_));
  }
#endif

  Profile* GetProfile() {
    return profile_ ? profile_.get() : browser()->profile();
  }

  virtual ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex(
      size_t index) = 0;

  SyncServiceImplHarness* sync_harness() {
    if (sync_harness_)
      return sync_harness_.get();

    sync_service()->OverrideNetworkForTest(
        fake_server::CreateFakeServerHttpPostProviderFactory(
            GetFakeServer()->AsWeakPtr()));
    sync_harness_ = SyncServiceImplHarness::Create(
        GetProfile(), "user@example.com", "password",
        SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
    return sync_harness_.get();
  }

  void EnableSync() {
    sync_harness()->SetupSync();
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
    ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());
  }

  syncer::SyncServiceImpl* sync_service() {
    return SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
        GetProfile());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(GetProfile());
  }

  void AdvanceFocus(int count) {
    for (int i = 0; i < count; i++)
      profile_menu_view()->GetFocusManager()->AdvanceFocus(
          /*reverse=*/false);
  }

  views::View* GetFocusedItem() {
    return profile_menu_view()->GetFocusManager()->GetFocusedView();
  }

  // This should be called in the test body.
  void RunTest() {
    ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());
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
        GetExpectedActionableItemAtIndex(GetParam()),
        /*expected_bucket_count=*/1);
  }

  base::CallbackListSubscription test_signin_client_subscription_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<SyncServiceImplHarness> sync_harness_;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
};

#define PROFILE_MENU_CLICK_TEST(actionable_item_list, test_case_name)     \
  class test_case_name : public ProfileMenuClickTest {                    \
   public:                                                                \
    test_case_name() = default;                                           \
    test_case_name(const test_case_name&) = delete;                       \
    test_case_name& operator=(const test_case_name&) = delete;            \
                                                                          \
    ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex( \
        size_t index) override {                                          \
      return actionable_item_list[index];                                 \
    }                                                                     \
  };                                                                      \
                                                                          \
  INSTANTIATE_TEST_SUITE_P(                                               \
      , test_case_name,                                                   \
      ::testing::Range(size_t(0), std::size(actionable_item_list)));      \
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

PROFILE_MENU_CLICK_TEST(kActionableItems_SingleProfileWithCustomName,
                        ProfileMenuClickTest_SingleProfileWithCustomName) {
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
  CreateAdditionalProfile();
  CreateAdditionalProfile();
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
  EnableSync();
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

// TODO(crbug.com/1298490): flaky on Windows and Mac
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ProfileMenuClickTest_SyncPaused \
  DISABLED_ProfileMenuClickTest_SyncPaused
#else
#define MAYBE_ProfileMenuClickTest_SyncPaused ProfileMenuClickTest_SyncPaused
#endif
PROFILE_MENU_CLICK_TEST(kActionableItems_SyncPaused,
                        MAYBE_ProfileMenuClickTest_SyncPaused) {
  EnableSync();
  sync_harness()->EnterSyncPausedStateForPrimaryAccount();
  // Check that the setup was successful.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_EQ(syncer::SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());

  RunTest();
}

// Lacros doesn't allow to disable sign-in in regular profiles yet.
// TODO(https://crbug.com/1220066): re-enable this test once kSigninAllowed is
// no longer force set to true on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
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
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
        // Signout is not allowed in the main profile.
        ProfileMenuViewBase::ActionableItem::kSignoutButton,
#endif
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_WithUnconsentedPrimaryAccount,
                        ProfileMenuClickTest_WithUnconsentedPrimaryAccount) {
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(), &test_url_loader_factory_, "user@example.com");
  UnconsentedPrimaryAccountChecker(identity_manager()).Wait();
  // Check that the setup was successful.
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  RunTest();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SecondaryProfileWithUnconsentedPrimaryAccount[] = {
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kSignoutButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(
    kActionableItems_SecondaryProfileWithUnconsentedPrimaryAccount,
    ProfileMenuClickTest_SecondaryProfileWithUnconsentedPrimaryAccount) {
  UseSecondaryProfile();
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(), &test_url_loader_factory_, "user@example.com");
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
    kActionableItems_UnconsentedPrimaryAccountError[] = {
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_UnconsentedPrimaryAccountError,
                        ProfileMenuClickTest_UnconsentedPrimaryAccountError) {
  AccountInfo account_info =
      signin::MakeAccountAvailable(identity_manager(), "user@example.com");
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), account_info.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_info.account_id, signin::ConsentLevel::kSignin);

  // Check that the setup was successful.
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_EQ(account_info, identity_manager()->GetPrimaryAccountInfo(
                              signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
// The two additional profiles created in the PRE_ test should be disabled and
// thus, not appear in  this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SecondaryProfilesDisabled[] = {
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SecondaryProfilesDisabled,
                        ProfileMenuClickTest_SecondaryProfilesDisabled) {
  // Check that the setup was successful.
  ASSERT_FALSE(g_browser_process->local_state()->GetBoolean(
      prefs::kLacrosSecondaryProfilesAllowed));

  RunTest();
}

IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_SecondaryProfilesDisabled,
                       PRE_ProfileMenuClickTest_SecondaryProfilesDisabled) {
  // Add two additional profiles, which later shouldn't be clickable.
  CreateAdditionalProfile();
  CreateAdditionalProfile();

  g_browser_process->local_state()->SetBoolean(
      prefs::kLacrosSecondaryProfilesAllowed, false);
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

PROFILE_MENU_CLICK_TEST(kActionableItems_GuestProfile,
                        ProfileMenuClickTest_GuestProfile) {
  SetTargetBrowser(CreateGuestBrowser());

  RunTest();
}
