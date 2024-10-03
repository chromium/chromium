// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "components/user_education/common/feature_promo_result.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/profiles/profile_menu_view.h"

#include <stddef.h>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
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
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
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
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_test_helper.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/supervised_user/core/browser/supervised_user_capabilities.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/webapps/common/web_app_id.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"

namespace {

constexpr char kTestEmail[] = "foo@example.com";

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
  Profile& profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);
  EXPECT_EQ(starting_number_of_profiles + 1,
            profile_manager->GetNumberOfProfiles());
  return &profile;
}

#if !BUILDFLAG(IS_CHROMEOS)

const char kPasswordManagerId[] = "chrome://password-manager/";
const char kPasswordManagerPWAUrl[] = "chrome://password-manager/?source=pwa";

std::unique_ptr<web_app::WebAppInstallInfo> CreatePasswordManagerWebAppInfo() {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>(
      webapps::ManifestId(kPasswordManagerId), GURL(kPasswordManagerPWAUrl));
  web_app_info->title = u"Password Manager";
  return web_app_info;
}

#endif

}  // namespace

class ProfileMenuViewTestBase {
 public:
 protected:
  void OpenProfileMenu() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(target_browser_);
    OpenProfileMenuFromToolbar(browser_view->toolbar_button_provider());
  }

  void OpenProfileMenuFromToolbar(ToolbarButtonProvider* toolbar) {
    // Click the avatar button to open the menu.
    views::View* avatar_button = toolbar->GetAvatarToolbarButton();
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
      views::test::WaitForWidgetActive(menu_widget, /*active=*/true);
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
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    clickable_view->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

 private:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> target_browser_ = nullptr;
};

class ProfileMenuViewExtensionsTest
    : public ProfileMenuViewTestBase,
      public InteractiveFeaturePromoTestT<extensions::ExtensionBrowserTest> {
 public:
  ProfileMenuViewExtensionsTest()
      : InteractiveFeaturePromoTestT(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHProfileSwitchFeature})) {}

  // InteractiveFeaturePromoTestT:
  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTestT::SetUpOnMainThread();
    SetTargetBrowser(browser());
  }
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
  base::RunLoop run_loop;
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHProfileSwitchFeature);
  params.show_promo_result_callback = base::BindLambdaForTesting(
      [&run_loop](user_education::FeaturePromoResult result) {
        ASSERT_TRUE(result);
        run_loop.Quit();
      });
  browser()->window()->MaybeShowFeaturePromo(std::move(params));
  run_loop.Run();
  EXPECT_TRUE(browser()->window()->IsFeaturePromoActive(
      feature_engagement::kIPHProfileSwitchFeature));

  // Open the menu.
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());

  // Check the IPH is no longer showing.
  EXPECT_FALSE(browser()->window()->IsFeaturePromoActive(
      feature_engagement::kIPHProfileSwitchFeature));
}

// Test that sets up a primary account (without sync) and simulates a click on
// the signout button.
class ProfileMenuViewSignoutTest : public ProfileMenuViewTestBase,
                                   public SigninBrowserTestBase {
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

  GURL GetExpectedLogoutURL(bool uno_enabled) const {
    if (uno_enabled) {
      return GaiaUrls::GetInstance()->LogOutURLWithContinueURL(GURL());
    } else {
      return GaiaUrls::GetInstance()->service_logout_url();
    }
  }

  // SigninBrowserTestBase:
  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    SetTargetBrowser(GetProfile() == browser()->profile()
                         ? browser()
                         : CreateBrowser(GetProfile()));

    // Add an account (no sync) with cookie.
    signin::AccountAvailabilityOptionsBuilder builder =
        identity_test_env()
            ->CreateAccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithCookie();
    CoreAccountInfo account_info =
        identity_test_env()->MakeAccountAvailable(builder.Build(kTestEmail));
    account_id_ = account_info.account_id;
    ASSERT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_));
    identity_test_env()->SetFreshnessOfAccountsInGaiaCookie(true);
  }

 private:
  CoreAccountId account_id_;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
};

// Wrapper class to add parametrized
// `switches::kExplicitBrowserSigninUIOnDesktop` feature tests.
class ProfileMenuViewSignoutTestWithExplicitBrowserSigninFeature
    : public ProfileMenuViewSignoutTest,
      public base::test::WithFeatureOverride {
 public:
  ProfileMenuViewSignoutTestWithExplicitBrowserSigninFeature()
      : base::test::WithFeatureOverride(
            switches::kExplicitBrowserSigninUIOnDesktop) {}

  bool uno_enabled() const { return IsParamFeatureEnabled(); }
};

// Checks that signout opens a new logout tab.
IN_PROC_BROWSER_TEST_P(
    ProfileMenuViewSignoutTestWithExplicitBrowserSigninFeature,
    OpenLogoutTab) {
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
  EXPECT_EQ(logout_page->GetURL(), GetExpectedLogoutURL(uno_enabled()));
  if (uno_enabled()) {
    EXPECT_FALSE(IdentityManagerFactory::GetForProfile(browser()->profile())
                     ->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }
}

// Checks that the NTP is navigated to the logout URL, instead of creating
// another tab.
IN_PROC_BROWSER_TEST_P(
    ProfileMenuViewSignoutTestWithExplicitBrowserSigninFeature,
    SignoutFromNTP) {
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
  EXPECT_EQ(logout_page->GetURL(), GetExpectedLogoutURL(uno_enabled()));
  if (uno_enabled()) {
    EXPECT_FALSE(IdentityManagerFactory::GetForProfile(browser()->profile())
                     ->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ProfileMenuViewSignoutTestWithExplicitBrowserSigninFeature);

// Signout test that handles logout requests. The parameter indicates whether
// an error page is generated for the logout request.
// Params of the ProfileMenuViewSignoutTestWithNetwork:
// -- bool uno_enabled;
// -- bool has_network_error;
class ProfileMenuViewSignoutTestWithNetwork
    : public ProfileMenuViewSignoutTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ProfileMenuViewSignoutTestWithNetwork()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ProfileMenuViewSignoutTestWithNetwork::HandleSignoutURL,
        has_network_error()));

    feature_list_.InitWithFeatureState(
        switches::kExplicitBrowserSigninUIOnDesktop, uno_enabled());
  }

  bool uno_enabled() const { return std::get<0>(GetParam()); }
  bool has_network_error() const { return std::get<1>(GetParam()); }

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

  static std::string GenerateTestSuffix(
      const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
    std::string suffix;
    suffix.append("Uno");
    suffix.append(std::get<0>(info.param) ? "Enabled" : "Disabled");
    suffix.append("AndNetwork");
    suffix.append(std::get<1>(info.param) ? "Off" : "On");
    return suffix;
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
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the local signout is performed (tokens are deleted) only if the
// logout tab failed to load.
IN_PROC_BROWSER_TEST_P(ProfileMenuViewSignoutTestWithNetwork, Signout) {
  // The test starts from about://blank, which causes the logout to happen in
  // the current tab.
  ASSERT_TRUE(Signout());
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* logout_page = tab_strip->GetActiveWebContents();
  EXPECT_EQ(logout_page->GetURL(), GetExpectedLogoutURL(uno_enabled()));

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
  if (uno_enabled()) {
    EXPECT_FALSE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfileMenuViewSignoutTestWithNetwork,
    testing::Combine(testing::Bool(), testing::Bool()),
    &ProfileMenuViewSignoutTestWithNetwork::GenerateTestSuffix);

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
        ->OnSyncErrorButtonClicked(AvatarSyncErrorType::kSyncPaused);
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
        identity_manager, kTestEmail, signin::ConsentLevel::kSync);
    signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);
    ASSERT_TRUE(
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info_.account_id));
  }

 private:
  CoreAccountInfo account_info_;
};

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
  EXPECT_THAT(
      reauth_page->GetURL().spec(),
      testing::StartsWith(GaiaUrls::GetInstance()->add_account_url().spec()));
}

#if !BUILDFLAG(IS_CHROMEOS)

class ProfileMenuViewSigninPendingTest : public ProfileMenuViewTestBase,
                                         public InProcessBrowserTest {
 public:
  ProfileMenuViewSigninPendingTest() = default;

  CoreAccountInfo account_info() const { return account_info_; }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetTargetBrowser(browser());

    // Add an account, non-syncing and in authentication error.
    Profile* profile = browser()->profile();
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    account_info_ = signin::MakePrimaryAccountAvailable(
        identity_manager, kTestEmail, signin::ConsentLevel::kSignin);
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_manager, account_info_.account_id,
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_SERVER));
    ASSERT_TRUE(
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info_.account_id));
    ASSERT_TRUE(profile->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
    ASSERT_FALSE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
    ASSERT_TRUE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }

  void ClickReauthButton() {
    base::HistogramTester histogram_tester;
    OpenProfileMenu();
    static_cast<ProfileMenuView*>(profile_menu_view())
        ->OnSigninButtonClicked(
            account_info(),
            ProfileMenuViewBase::ActionableItem::kSigninReauthButton,
            signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN);
    histogram_tester.ExpectUniqueSample(
        "Profile.Menu.ClickedActionableItem",
        ProfileMenuViewBase::ActionableItem::kSigninReauthButton,
        /*expected_bucket_count=*/1);
  }

 protected:
  CoreAccountInfo account_info_;
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

IN_PROC_BROWSER_TEST_F(ProfileMenuViewSigninPendingTest, OpenReauthTab) {
  // Start from a page that is not the NTP, so that the reauth opens a new tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.google.com")));

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  ClickReauthButton();
  content::WebContents* reauth_page = tab_waiter.Wait();
  std::string reauth_url = reauth_page->GetURL().spec();
  // The signin page opens (not the sync page).
  EXPECT_THAT(
      reauth_url,
      testing::StartsWith(GaiaUrls::GetInstance()->add_account_url().spec()));
  // The email is pre-filled.
  EXPECT_THAT(reauth_url, testing::HasSubstr(base::EscapeQueryParamValue(
                              account_info_.email, true)));
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

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
    ASSERT_TRUE(sync_harness()->SetupSync());
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

    // A HoverButton may have focused itself if the mouse happened to be over it
    // when it became visible. Clear the focus now to ensure that we advance to
    // the right item.
    profile_menu_view()->GetFocusManager()->ClearFocus();

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

    if (supervised_user::IsPrimaryAccountSubjectToParentalControls(
            identity_manager()) == signin::Tribool::kTrue) {
      histogram_tester_.ExpectUniqueSample(
          "Profile.Menu.ClickedActionableItem_Supervised",
          GetExpectedActionableItemAtIndex(GetParam()),
          /*expected_bucket_count=*/1);
    } else {
      histogram_tester_.ExpectUniqueSample(
          "Profile.Menu.ClickedActionableItem_Supervised",
          GetExpectedActionableItemAtIndex(GetParam()),
          /*expected_bucket_count=*/0);
    }
  }

  base::CallbackListSubscription test_signin_client_subscription_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<SyncServiceImplHarness> sync_harness_;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
};

#define PROFILE_MENU_CLICK_TEST_WITH_FEATURE_STATES_F(                    \
    FixtureClass, actionable_item_list, test_case_name, enabled_features, \
    disabled_features)                                                    \
  class test_case_name : public FixtureClass {                            \
   public:                                                                \
    test_case_name() {                                                    \
      scoped_feature_list_##test_case_name.InitWithFeatures(              \
          enabled_features, disabled_features);                           \
    }                                                                     \
    test_case_name(const test_case_name&) = delete;                       \
    test_case_name& operator=(const test_case_name&) = delete;            \
                                                                          \
    ProfileMenuViewBase::ActionableItem GetExpectedActionableItemAtIndex( \
        size_t index) override {                                          \
      return actionable_item_list[index];                                 \
    }                                                                     \
                                                                          \
   private:                                                               \
    base::test::ScopedFeatureList scoped_feature_list_##test_case_name;   \
  };                                                                      \
                                                                          \
  INSTANTIATE_TEST_SUITE_P(                                               \
      , test_case_name,                                                   \
      ::testing::Range(size_t(0), std::size(actionable_item_list)));      \
                                                                          \
  IN_PROC_BROWSER_TEST_P(test_case_name, test_case_name)

// Specialized variant of `PROFILE_MENU_CLICK_TEST_WITH_FEATURE_STATES_F` with
// no features overrides.
#define PROFILE_MENU_CLICK_TEST_F(FixtureClass, actionable_item_list, \
                                  test_case_name)                     \
  PROFILE_MENU_CLICK_TEST_WITH_FEATURE_STATES_F(                      \
      FixtureClass, actionable_item_list, test_case_name, {}, {})

// Specialized variant of `PROFILE_MENU_CLICK_TEST_WITH_FEATURE_STATES_F` using
// `ProfileMenuClickTest` as `FixtureClass`, and allowing to override features
// states.
#define PROFILE_MENU_CLICK_WITH_FEATURE_TEST(                                  \
    actionable_item_list, test_case_name, enabled_features, disabled_features) \
  PROFILE_MENU_CLICK_TEST_WITH_FEATURE_STATES_F(                               \
      ProfileMenuClickTest, actionable_item_list, test_case_name,              \
      enabled_features, disabled_features)

// Specialized variant of `PROFILE_MENU_CLICK_WITH_FEATURE_TEST` with no
// features overrides, which uses `ProfileMenuClickTest` as fixture class.
#define PROFILE_MENU_CLICK_TEST(actionable_item_list, test_case_name)        \
  PROFILE_MENU_CLICK_WITH_FEATURE_TEST(actionable_item_list, test_case_name, \
                                       {}, {})

// List of actionable items in the correct order as they appear in the menu with
// Uno disabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SingleProfileWithCustomName_UnoDisabled[] = {
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

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SingleProfileWithCustomName_UnoDisabled,
    ProfileMenuClickTest_SingleProfileWithCustomName_UnoDisabled,
    /*enabled_features=*/{},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
  profiles::UpdateProfileName(browser()->profile(), u"Custom name");
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno enabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SingleProfileWithCustomName_UnoEnabled[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SingleProfileWithCustomName_UnoEnabled,
    ProfileMenuClickTest_SingleProfileWithCustomName_UnoEnabled,
    /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
    /*disabled_features=*/{}) {
  profiles::UpdateProfileName(browser()->profile(), u"Custom name");
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno disabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_MultipleProfiles_UnoDisabled[] = {
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

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_MultipleProfiles_UnoDisabled,
    ProfileMenuClickTest_MultipleProfiles_UnoDisabled,
    /*enabled_features=*/{},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
  // Add two additional profiles.
  CreateAdditionalProfile();
  CreateAdditionalProfile();
  // Open a second browser window for the current profile, so the
  // ExitProfileButton is shown.
  SetTargetBrowser(CreateBrowser(browser()->profile()));
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno enabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_MultipleProfiles_UnoEnabled[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
        ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_MultipleProfiles_UnoEnabled,
    ProfileMenuClickTest_MultipleProfiles_UnoEnabled,
    /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
    /*disabled_features=*/{}) {
  // Add two additional profiles.
  CreateAdditionalProfile();
  CreateAdditionalProfile();
  // Open a second browser window for the current profile, so the
  // ExitProfileButton is shown.
  SetTargetBrowser(CreateBrowser(browser()->profile()));
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno disabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SyncEnabled_UnoDisabled[] = {
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

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SyncEnabled_UnoDisabled,
    ProfileMenuClickTest_SyncEnabled_UnoDisabled,
    /*enabled_features=*/{},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
  EnableSync();
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno enabled. If a new button is added to the menu, it should also be added to
// this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SyncEnabled_UnoEnabled[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

// TODO(crbug.com/341975308): re-enable test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ProfileMenuClickTest_SyncEnabled_UnoEnabled \
  DISABLED_ProfileMenuClickTest_SyncEnabled_UnoEnabled
#else
#define MAYBE_ProfileMenuClickTest_SyncEnabled_UnoEnabled \
  ProfileMenuClickTest_SyncEnabled_UnoEnabled
#endif
PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SyncEnabled_UnoEnabled,
    MAYBE_ProfileMenuClickTest_SyncEnabled_UnoEnabled,
    /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
    /*disabled_features=*/{}) {
  EnableSync();
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno disabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SyncError_UnoDisabled[] = {
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

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SyncError_UnoDisabled,
    ProfileMenuClickTest_SyncError_UnoDisabled,
    /*enabled_features=*/{},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
  ASSERT_TRUE(
      sync_harness()->SignInPrimaryAccount(signin::ConsentLevel::kSync));
  // Check that the setup was successful.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_FALSE(sync_service()->IsSyncFeatureEnabled());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno enabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SyncError_UnoEnabled[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSyncErrorButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SyncError_UnoEnabled,
    ProfileMenuClickTest_SyncError_UnoEnabled,
    /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
    /*disabled_features=*/{}) {
  ASSERT_TRUE(
      sync_harness()->SignInPrimaryAccount(signin::ConsentLevel::kSync));
  // Check that the setup was successful.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_FALSE(sync_service()->IsSyncFeatureEnabled());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno disabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SyncPaused_UnoDisabled[] = {
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

// TODO(crbug.com/40822972): flaky on Windows and Mac
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ProfileMenuClickTest_SyncPaused_UnoDisabled \
  DISABLED_ProfileMenuClickTest_SyncPaused_UnoDisabled
#else
#define MAYBE_ProfileMenuClickTest_SyncPaused_UnoDisabled \
  ProfileMenuClickTest_SyncPaused_UnoDisabled
#endif
PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SyncPaused_UnoDisabled,
    MAYBE_ProfileMenuClickTest_SyncPaused_UnoDisabled,
    /*enabled_features=*/{},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
  EnableSync();
  sync_harness()->EnterSyncPausedStateForPrimaryAccount();
  // Check that the setup was successful.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_EQ(syncer::SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno enabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SyncPaused_UnoEnabled[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSyncErrorButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

// TODO(crbug.com/40822972): flaky on Windows and Mac
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ProfileMenuClickTest_SyncPaused_UnoEnabled \
  DISABLED_ProfileMenuClickTest_SyncPaused_UnoEnabled
#else
#define MAYBE_ProfileMenuClickTest_SyncPaused_UnoEnabled \
  ProfileMenuClickTest_SyncPaused_UnoEnabled
#endif
PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SyncPaused_UnoEnabled,
    MAYBE_ProfileMenuClickTest_SyncPaused_UnoEnabled,
    /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
    /*disabled_features=*/{}) {
  EnableSync();
  sync_harness()->EnterSyncPausedStateForPrimaryAccount();
  // Check that the setup was successful.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_EQ(syncer::SyncService::TransportState::PAUSED,
            sync_service()->GetTransportState());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Uno disabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SigninDisallowed_UnoDisabled[] = {
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

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SigninDisallowed_UnoDisabled,
    ProfileMenuClickTest_SigninDisallowed_UnoDisabled,
    /*enabled_features=*/{},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
  // Check that the setup was successful.
  ASSERT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed));

  RunTest();
}

IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_SigninDisallowed_UnoDisabled,
                       PRE_ProfileMenuClickTest_SigninDisallowed_UnoDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSigninAllowedOnNextStartup, false);
}

// List of actionable items in the correct order as they appear in the menu with
// Uno enabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_SigninDisallowed_UnoEnabled[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_SigninDisallowed_UnoEnabled,
    ProfileMenuClickTest_SigninDisallowed_UnoEnabled,
    /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
    /*disabled_features=*/{}) {
  // Check that the setup was successful.
  ASSERT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed));

  RunTest();
}

IN_PROC_BROWSER_TEST_P(ProfileMenuClickTest_SigninDisallowed_UnoEnabled,
                       PRE_ProfileMenuClickTest_SigninDisallowed_UnoEnabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSigninAllowedOnNextStartup, false);
}

// List of actionable items in the correct order as they appear in the menu with
// Uno disabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_WithUnconsentedPrimaryAccount_UnoDisabled[] = {
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

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_WithUnconsentedPrimaryAccount_UnoDisabled,
    ProfileMenuClickTest_WithUnconsentedPrimaryAccount_UnoDisabled,
    /*enabled_features=*/{},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
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

// List of actionable items in the correct order as they appear in the menu with
// Uno enabled. If a new button is added to the menu, it should also be added
// to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_WithUnconsentedPrimaryAccount_UnoEnabled[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kSignoutButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_WithUnconsentedPrimaryAccount_UnoEnabled,
    ProfileMenuClickTest_WithUnconsentedPrimaryAccount_UnoEnabled,
    /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
    /*disabled_features=*/{}) {
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

// List of actionable items in the correct order as they appear in the menu with
// Uno enabled in signin pending state. If a new button is added to the menu,
// it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_WithPendingAccount_UnoEnabled[] = {
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninReauthButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kExitProfileButton,
        ProfileMenuViewBase::ActionableItem::kSignoutButton,
        ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kPasswordsButton};

// TODO(crbug.com/40822972): flaky on Windows and Mac
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ProfileMenuClickTest_WithPendingAccount_UnoEnabled \
  DISABLED_ProfileMenuClickTest_WithPendingAccount_UnoEnabled
#else
#define MAYBE_ProfileMenuClickTest_WithPendingAccount_UnoEnabled \
  ProfileMenuClickTest_WithPendingAccount_UnoEnabled
#endif
PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_WithPendingAccount_UnoEnabled,
    MAYBE_ProfileMenuClickTest_WithPendingAccount_UnoEnabled,
    /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
    /*disabled_features=*/{}) {
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "user@example.com", signin::ConsentLevel::kSignin);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), account_info.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
  UnconsentedPrimaryAccountChecker(identity_manager()).Wait();
  // Check that the setup was successful.
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  ASSERT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_info.account_id));

  RunTest();
}

constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_GuestProfileButtonAvailable_SignedInNotSupervised[] = {
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

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_GuestProfileButtonAvailable_SignedInNotSupervised,
    ProfileMenuClickTest_GuestProfileButtonAvailable_SignedInNotSupervised,
    /*enabled_features=*/{supervised_user::kHideGuestModeForSupervisedUsers},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "adult@gmail.com", signin::ConsentLevel::kSignin);
  supervised_user::UpdateSupervisionStatusForAccount(
      account_info, identity_manager(),
      /*is_subject_to_parental_controls=*/false);
  UnconsentedPrimaryAccountChecker(identity_manager()).Wait();

  // Check setup.
  ASSERT_EQ(account_info.account_id, identity_manager()->GetPrimaryAccountId(
                                         signin::ConsentLevel::kSignin));
  ASSERT_TRUE(profiles::IsGuestModeEnabled(*GetProfile()));

  RunTest();
}

constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_GuestProfileButtonNotAvailable_SignedInSupervised[] = {
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kPasswordsButton,
        ProfileMenuViewBase::ActionableItem::kCreditCardsButton,
        ProfileMenuViewBase::ActionableItem::kAddressesButton,
        ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kSignoutButton,
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The kGuestProfileButton entry is not present.
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kEditProfileButton};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_GuestProfileButtonNotAvailable_SignedInSupervised,
    ProfileMenuClickTest_GuestProfileButtonNotAvailable_SignedInSupervised,
    /*enabled_features=*/{supervised_user::kHideGuestModeForSupervisedUsers},
    /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop}) {
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "child@gmail.com", signin::ConsentLevel::kSignin);
  supervised_user::UpdateSupervisionStatusForAccount(
      account_info, identity_manager(),
      /*is_subject_to_parental_controls=*/true);
  UnconsentedPrimaryAccountChecker(identity_manager()).Wait();

  // Check setup.
  ASSERT_EQ(account_info.account_id, identity_manager()->GetPrimaryAccountId(
                                         signin::ConsentLevel::kSignin));
  ASSERT_FALSE(profiles::IsGuestModeEnabled(*GetProfile()));

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

PROFILE_MENU_CLICK_TEST(kActionableItems_GuestProfile,
                        ProfileMenuClickTest_GuestProfile) {
  SetTargetBrowser(CreateGuestBrowser());

  RunTest();
}

#if !BUILDFLAG(IS_CHROMEOS)
class ProfileMenuClickTestWebApp : public ProfileMenuClickTest {
 protected:
  void SetUpOnMainThread() override {
    ProfileMenuClickTest::SetUpOnMainThread();

    // OS integration is needed to be able to launch web applications. This
    // override ensures OS integration doesn't leave any traces.
    override_registration_ =
        web_app::OsIntegrationTestOverrideImpl::OverrideForTesting();
  }

  void TearDownOnMainThread() override {
    for (Profile* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      web_app::test::UninstallAllWebApps(profile);
    }
    override_registration_.reset();
    ProfileMenuClickTest::TearDownOnMainThread();
  }

  WebAppFrameToolbarTestHelper& toolbar_helper() {
    return web_app_frame_toolbar_helper_;
  }

 private:
  // OS integration is needed to be able to launch web applications. This
  // override ensures OS integration doesn't leave any traces.
  std::unique_ptr<web_app::OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
};

// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
constexpr ProfileMenuViewBase::ActionableItem
    kActionableItems_PasswordManagerWebApp[] = {
        ProfileMenuViewBase::ActionableItem::kOtherProfileButton};

PROFILE_MENU_CLICK_TEST_F(ProfileMenuClickTestWebApp,
                          kActionableItems_PasswordManagerWebApp,
                          ProfileMenuClickTest_PasswordManagerWebApp) {
  // Add an additional profile.
  CreateAdditionalProfile();

  // Install and launch an application for the first profile.
  webapps::AppId app_id = toolbar_helper().InstallAndLaunchCustomWebApp(
      browser(), CreatePasswordManagerWebAppInfo(),
      GURL(kPasswordManagerPWAUrl));
  SetTargetBrowser(toolbar_helper().app_browser());
  RunTest();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
// List of actionable items in the correct order as they appear in the menu.
// If a new button is added to the menu, it should also be added to this list.
// Unfortunately by how Click Tests work we can't verify how many other profile
// buttons are present, so this test merely verifies that at least one exists.
constexpr ProfileMenuViewBase::ActionableItem kActionableItems_RegularWebApp[] =
    {ProfileMenuViewBase::ActionableItem::kOtherProfileButton};
PROFILE_MENU_CLICK_TEST_F(ProfileMenuClickTestWebApp,
                          kActionableItems_RegularWebApp,
                          ProfileMenuClickTest_RegularWebApp) {
  // Add an additional profile.
  Profile* profile1 = GetProfile();
  Profile* profile2 = CreateAdditionalProfile();

  // Install and launch an application in profile1 and also install the same
  // app in profile2.
  webapps::AppId app_id = toolbar_helper().InstallAndLaunchWebApp(
      profile1, GURL("https://test.org"));
  SetTargetBrowser(toolbar_helper().app_browser());
  EXPECT_EQ(app_id,
            toolbar_helper().InstallWebApp(profile2, GURL("https://test.org")));

  RunTest();
}
#endif

#if !BUILDFLAG(IS_CHROMEOS)
class ProfileMenuViewWebAppTest : public ProfileMenuViewTestBase,
                                  public web_app::WebAppBrowserTestBase {
 protected:
  void TearDownOnMainThread() override {
    for (Profile* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      web_app::test::UninstallAllWebApps(profile);
    }
    web_app::WebAppBrowserTestBase::TearDownOnMainThread();
  }

  WebAppFrameToolbarTestHelper& toolbar_helper() {
    return web_app_frame_toolbar_helper_;
  }

 private:
  WebAppFrameToolbarTestHelper web_app_frame_toolbar_helper_;
};

IN_PROC_BROWSER_TEST_F(ProfileMenuViewWebAppTest,
                       SelectingOtherProfilePasswordManager) {
  // Create a second profile.
  Profile* second_profile = CreateAdditionalProfile();
  web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
      web_app::WebAppProvider::GetForTest(second_profile));
  ASSERT_FALSE(chrome::FindBrowserWithProfile(second_profile));

  // Install and launch an application for the first profile.
  webapps::AppId app_id = toolbar_helper().InstallAndLaunchCustomWebApp(
      browser(), CreatePasswordManagerWebAppInfo(),
      GURL(kPasswordManagerPWAUrl));
  SetTargetBrowser(toolbar_helper().app_browser());

  // Open profile menu.
  auto* toolbar =
      toolbar_helper().browser_view()->web_app_frame_toolbar_for_testing();
  ASSERT_TRUE(toolbar);
  OpenProfileMenuFromToolbar(toolbar);

  // Select other profile by advancing the focus one step forward
  profile_menu_view()->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
  auto* focused_item = profile_menu_view()->GetFocusManager()->GetFocusedView();
  ASSERT_TRUE(focused_item);

  // Wait for the new app window to be open for the second profile.
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  Click(focused_item);
  content::WebContents* new_web_contents = waiter.Wait();
  ASSERT_TRUE(new_web_contents);
  Browser* new_browser = chrome::FindBrowserWithProfile(second_profile);
  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(new_browser->is_type_app());
  EXPECT_EQ(new_browser->tab_strip_model()->GetActiveWebContents(),
            new_web_contents);
  EXPECT_EQ(new_web_contents->GetVisibleURL(), GURL(kPasswordManagerPWAUrl));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ProfileMenuViewWebAppTest, SelectingOtherProfile) {
  // Add additional profiles.
  Profile* profile1 = profile();
  Profile* profile2 = CreateAdditionalProfile();
  Profile* profile3 = CreateAdditionalProfile();

  // Install an application in first and third profiles, launching only in the
  // first profile.
  webapps::AppId app_id = toolbar_helper().InstallAndLaunchWebApp(
      profile1, GURL("https://test.org"));
  EXPECT_EQ(app_id,
            toolbar_helper().InstallWebApp(profile3, GURL("https://test.org")));
  SetTargetBrowser(toolbar_helper().app_browser());
  EXPECT_FALSE(chrome::FindBrowserWithProfile(profile3));

  // Open profile menu in first profile.
  auto* toolbar =
      toolbar_helper().browser_view()->web_app_frame_toolbar_for_testing();
  ASSERT_TRUE(toolbar);
  OpenProfileMenuFromToolbar(toolbar);

  // Select third profile by advancing the focus one step forward.
  profile_menu_view()->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
  auto* focused_item = profile_menu_view()->GetFocusManager()->GetFocusedView();
  ASSERT_TRUE(focused_item);

  // Wait for the new app window to be open for the third profile.
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  Click(focused_item);
  content::WebContents* new_web_contents = waiter.Wait();
  ASSERT_TRUE(new_web_contents);
  EXPECT_FALSE(chrome::FindBrowserWithProfile(profile2));
  Browser* new_browser = chrome::FindBrowserWithProfile(profile3);
  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(new_browser->is_type_app());
  EXPECT_EQ(new_browser->tab_strip_model()->GetActiveWebContents(),
            new_web_contents);
  EXPECT_EQ(new_web_contents->GetVisibleURL(), GURL("https://test.org"));
}

IN_PROC_BROWSER_TEST_F(ProfileMenuViewWebAppTest, ProfileMenuVisibility) {
  // Add an additional profile.
  Profile* profile1 = profile();
  Profile* profile2 = CreateAdditionalProfile();

  // Install and launch an application in first profile.
  webapps::AppId app_id = toolbar_helper().InstallAndLaunchWebApp(
      profile1, GURL("https://test.org"));

  // Verify that avatar button is not visible.
  auto* toolbar_profile1 =
      toolbar_helper().browser_view()->web_app_frame_toolbar_for_testing();
  ASSERT_TRUE(toolbar_profile1);
  ASSERT_TRUE(toolbar_profile1->GetAvatarToolbarButton());
  EXPECT_FALSE(toolbar_profile1->GetAvatarToolbarButton()->GetVisible());

  // Now install and launch application in second profile.
  EXPECT_EQ(app_id, toolbar_helper().InstallAndLaunchWebApp(
                        profile2, GURL("https://test.org")));

  // Avatar button should be visible in both profiles.
  EXPECT_TRUE(toolbar_profile1->GetAvatarToolbarButton()->GetVisible());
  auto* toolbar_profile2 =
      toolbar_helper().browser_view()->web_app_frame_toolbar_for_testing();
  ASSERT_TRUE(toolbar_profile2);
  ASSERT_TRUE(toolbar_profile2->GetAvatarToolbarButton());
  EXPECT_TRUE(toolbar_profile2->GetAvatarToolbarButton()->GetVisible());
}
#endif  // BUILDFLAG(IS_MAC)
