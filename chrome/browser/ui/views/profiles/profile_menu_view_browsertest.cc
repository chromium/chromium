// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view.h"

#include <stddef.h>

#include <array>

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
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
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
#include "chrome/browser/signin/signin_ui_delegate.h"
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
#include "chrome/browser/ui/ui_features.h"
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
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/supervised_user/core/browser/family_link_user_capabilities.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace {
using ::testing::StrictMock;

constexpr char kTestEmail[] = "foo@example.com";

class MockSigninUiDelegate : public signin_ui_util::SigninUiDelegate {
 public:
  MOCK_METHOD(void,
              ShowTurnSyncOnUI,
              (Profile*,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction,
               const CoreAccountId&,
               TurnSyncOnHelper::SigninAbortedMode,
               bool,
               bool),
              (override));

  MOCK_METHOD(void,
              ShowSigninUI,
              (Profile*,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile*,
               const std::string&,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
};

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

void Click(views::View* clickable_view) {
  // Simulate a mouse click. Note: Buttons are either fired when pressed or
  // when released, so the corresponding methods need to be called.
  clickable_view->OnMousePressed(
      ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  clickable_view->OnMouseReleased(
      ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
}

void WaitForMenuToBeActive(ProfileMenuViewBase* profile_menu_view) {
  ASSERT_TRUE(profile_menu_view);
  profile_menu_view->set_close_on_deactivate(false);
#if BUILDFLAG(IS_MAC)
  base::RunLoop().RunUntilIdle();
#else
  views::Widget* menu_widget = profile_menu_view->GetWidget();
  ASSERT_TRUE(menu_widget);
  if (menu_widget->CanActivate()) {
    views::test::WaitForWidgetActive(menu_widget, /*active=*/true);
  } else {
    LOG(ERROR) << "menu_widget can not be activated";
  }
#endif
}

}  // namespace

class ProfileMenuViewTestBase {
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
    ASSERT_NO_FATAL_FAILURE(WaitForMenuToBeActive(profile_menu_view()));
  }

  ProfileMenuViewBase* profile_menu_view() {
    auto* coordinator = ProfileMenuCoordinator::FromBrowser(target_browser_);
    return coordinator ? coordinator->GetProfileMenuViewBaseForTesting()
                       : nullptr;
  }
  void SetTargetBrowser(Browser* browser) { target_browser_ = browser; }

 private:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> target_browser_ = nullptr;
};

class ProfileMenuViewExtensionsTest
    : public ProfileMenuViewTestBase,
      public InteractiveFeaturePromoTestT<extensions::ExtensionBrowserTest> {
 public:
  ProfileMenuViewExtensionsTest()
      : InteractiveFeaturePromoTestT(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHProfileSwitchFeature,
             feature_engagement::kIPHSupervisedUserProfileSigninFeature})) {}

  // InteractiveFeaturePromoTestT:
  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTestT::SetUpOnMainThread();
    SetTargetBrowser(browser());
  }
};

IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest, RootViewAccessibleName) {
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());

  // The theme change destroys the avatar button. Make sure the profile chooser
  // widget doesn't try to reference a stale observer during its shutdown.
  test::ThemeServiceChangedWaiter waiter(
      ThemeServiceFactory::GetForProfile(profile()));
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  waiter.WaitForThemeChanged();

  auto* coordinator = ProfileMenuCoordinator::FromBrowser(browser());
  EXPECT_TRUE(coordinator->IsShowing());

  ui::AXNodeData root_view_data;
  profile_menu_view()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&root_view_data);
  EXPECT_EQ(
      root_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      profile_menu_view()->GetAccessibleWindowTitle());
}

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

// Profile chooser view should close when the last tab is closed.
// Regression test for http://crbug.com/792845
IN_PROC_BROWSER_TEST_F(ProfileMenuViewExtensionsTest,
                       CloseBubbleOnLastTabClosed) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip->count());
  ASSERT_EQ(0, tab_strip->active_index());

  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());

  // Wait for browser widget destruction. Below will crash due to raw_ptr
  // detection if the profile bubble outlives the browser.
  tab_strip->CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
  views::test::WidgetDestroyedWaiter(browser()->GetBrowserView().GetWidget())
      .Wait();
}

// Opening the profile menu dismisses any existing IPH.
// Regression test for crbug.com/1205901 (Profile Switch IPH)
// and for crbug.com/378449081 (Supervised User IPH).
class ProfileMenuViewExtensionsIphDismissTest
    : public ProfileMenuViewExtensionsTest,
      public testing::WithParamInterface<base::test::FeatureRef> {
 public:
  const base::Feature& GetIphFeature() { return *GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ProfileMenuViewExtensionsIphDismissTest,
    testing::Values(
        base::test::FeatureRef(feature_engagement::kIPHProfileSwitchFeature),
        base::test::FeatureRef(
            feature_engagement::kIPHSupervisedUserProfileSigninFeature)),
    [](const auto& info) {
      return info.param == feature_engagement::kIPHProfileSwitchFeature
                 ? "_ProfileSwitch"
                 : "_SupervisedUser";
    });

IN_PROC_BROWSER_TEST_P(ProfileMenuViewExtensionsIphDismissTest, CloseIPH) {
  // Display the IPH.
  base::RunLoop run_loop;
  user_education::FeaturePromoParams params(GetIphFeature());
  params.show_promo_result_callback = base::BindLambdaForTesting(
      [&run_loop](user_education::FeaturePromoResult result) {
        ASSERT_TRUE(result);
        run_loop.Quit();
      });
  browser()->window()->MaybeShowFeaturePromo(std::move(params));
  run_loop.Run();
  EXPECT_TRUE(browser()->window()->IsFeaturePromoActive(GetIphFeature()));

  // Open the menu.
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenu());

  // Check the IPH is no longer showing.
  EXPECT_FALSE(browser()->window()->IsFeaturePromoActive(GetIphFeature()));
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
    if (HasFatalFailure()) {
      return false;
    }

    std::unique_ptr<content::TestNavigationObserver> observer;

    // Note: the signout dialog is only meant to be shown for DICE enabled
    // users. See SigninViewController::ShowSignoutConfirmationPrompt.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    auto url = GURL(chrome::kChromeUISignoutConfirmationURL);
    observer = std::make_unique<content::TestNavigationObserver>(url);
    observer->StartWatchingNewWebContents();
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

    static_cast<ProfileMenuView*>(profile_menu_view())
        ->OnSignoutButtonClicked();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    if (observer.get()) {
      observer->Wait();
      auto* signin_view_controller = browser()->signin_view_controller();
      auto* signout_ui = SignoutConfirmationUI::GetForTesting(
          signin_view_controller->GetModalDialogWebContentsForTesting());
      if (!signout_ui) {
        return false;
      }
      // Click "Sign Out Anyway".
      signout_ui->AcceptDialogForTesting();
    }
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

    return true;
  }

  GURL GetExpectedLogoutURL() const {
    return GaiaUrls::GetInstance()->LogOutURLWithContinueURL(GURL());
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
  EXPECT_EQ(logout_page->GetURL(), GetExpectedLogoutURL());
  EXPECT_FALSE(IdentityManagerFactory::GetForProfile(browser()->profile())
                   ->HasPrimaryAccount(signin::ConsentLevel::kSignin));
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
  EXPECT_EQ(logout_page->GetURL(), GetExpectedLogoutURL());
  EXPECT_FALSE(IdentityManagerFactory::GetForProfile(browser()->profile())
                   ->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

// Signout test that handles logout requests. The parameter indicates whether
// an error page is generated for the logout request.
// Params of the ProfileMenuViewSignoutTestWithNetwork:
// -- bool has_network_error;
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

  static std::string GenerateTestSuffix(
      const testing::TestParamInfo<bool>& info) {
    std::string suffix;
    suffix.append("Network");
    suffix.append(info.param ? "Off" : "On");
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
  EXPECT_EQ(logout_page->GetURL(), GetExpectedLogoutURL());

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
  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfileMenuViewSignoutTestWithNetwork,
    testing::Bool(),
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
    if (HasFatalFailure()) {
      return false;
    }
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

class ProfileMenuViewWebOnlyTest : public ProfileMenuViewTestBase,
                                   public SigninBrowserTestBase {
 public:
  // SigninBrowserTestBase:
  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    SetTargetBrowser(browser());

    // Add an account, not signed in.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    account_info_ = identity_test_env()->MakeAccountAvailable(
        kTestEmail,
        {.primary_account_consent_level = std::nullopt, .set_cookie = true});

    ASSERT_FALSE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    ASSERT_EQ(identity_manager->GetAccountsWithRefreshTokens().size(), 1u);
  }

  void ClickSigninButton() {
    base::HistogramTester histogram_tester;
    OpenProfileMenu();

    // Select the signin button by advancing the focus.
    profile_menu_view()->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
    auto* focused_item =
        profile_menu_view()->GetFocusManager()->GetFocusedView();
    ASSERT_TRUE(focused_item);
    Click(focused_item);

    histogram_tester.ExpectUniqueSample(
        "Profile.Menu.ClickedActionableItem",
        ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
        /*expected_bucket_count=*/1);
  }

  CoreAccountInfo account_info_;
};

// Checks that the signin flow starts in one click.
IN_PROC_BROWSER_TEST_F(ProfileMenuViewWebOnlyTest, ContinueAs) {
  StrictMock<MockSigninUiDelegate> mock_signin_ui_delegate;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset =
      signin_ui_util::SetSigninUiDelegateForTesting(&mock_signin_ui_delegate);
  base::HistogramTester histogram_tester;
  const signin_metrics::AccessPoint expected_access_point =
      signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo;
  EXPECT_CALL(
      mock_signin_ui_delegate,
      ShowTurnSyncOnUI(browser()->profile(), expected_access_point,
                       signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
                       account_info_.account_id,
                       TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
                       /*is_sync_promo=*/true,
                       /*turn_sync_on_signed_profile=*/false));
  ClickSigninButton();
  // `Signin.SyncOptIn.Offered` should NOT be recorded if the sync opt-in is
  // not directly offered from the profile menu.
  histogram_tester.ExpectUniqueSample("Signin.SyncOptIn.Offered",
                                      expected_access_point,
                                      /*expected_bucket_count=*/0);
  // `Signin.SignIn.Offered*` should be recorded if the sign-in is offered from
  // the profile menu.
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Offered",
                                      expected_access_point,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Offered.WithDefault",
                                      expected_access_point,
                                      /*expected_bucket_count=*/1);
}

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
            signin_metrics::AccessPoint::kAvatarBubbleSignIn);
    histogram_tester.ExpectUniqueSample(
        "Profile.Menu.ClickedActionableItem",
        ProfileMenuViewBase::ActionableItem::kSigninReauthButton,
        /*expected_bucket_count=*/1);
  }

 protected:
  CoreAccountInfo account_info_;
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
    if (sync_harness_) {
      return sync_harness_.get();
    }

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
    for (int i = 0; i < count; i++) {
      profile_menu_view()->GetFocusManager()->AdvanceFocus(
          /*reverse=*/false);
    }
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
      scoped_feature_list_##test_case_name.InitWithFeaturesAndParameters( \
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

// List of actionable items in the correct order as they appear in the menu. If
// a new button is added to the menu, it should also be added to this list.
constexpr std::array kActionableItems_SingleProfileWithCustomName = {
    ProfileMenuViewBase::ActionableItem::kSigninButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kSigninButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SingleProfileWithCustomName,
                        ProfileMenuClickTest_SingleProfileWithCustomName) {
  profiles::UpdateProfileName(browser()->profile(), u"Custom name");
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu. If
// a new button is added to the menu, it should also be added to this list.
constexpr std::array kActionableItems_ManagedProfile = {
    ProfileMenuViewBase::ActionableItem::kProfileManagementLabel,
    ProfileMenuViewBase::ActionableItem::kSigninButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kProfileManagementLabel};

const std::vector<base::test::FeatureRefAndParams>
    kManagedProfileEnabledFeatures = {
        {features::kEnterpriseProfileBadgingForMenu, {}}};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(kActionableItems_ManagedProfile,
                                     ProfileMenuClickTest_ManagedProfile,
                                     kManagedProfileEnabledFeatures,
                                     /*disabled_features=*/{}) {
  enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(), true);
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      scoped_browser_management_ =
          std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
              policy::ManagementServiceFactory::GetForProfile(
                  browser()->profile()),
              policy::EnterpriseManagementAuthority::CLOUD);
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu. If
// a new button is added to the menu, it should also be added to this list.
constexpr std::array kActionableItems_MultipleProfiles = {
    ProfileMenuViewBase::ActionableItem::kSigninButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kExitProfileButton,
    ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
    ProfileMenuViewBase::ActionableItem::kOtherProfileButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kSigninButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_MultipleProfiles,
                        ProfileMenuClickTest_MultipleProfiles) {
  // Add two additional profiles.
  Profile* other_profile = CreateAdditionalProfile();
  CreateAdditionalProfile();
  // Open a browser for another profile, and a second browser for the current
  // profile, so the kExitProfileButton is shown.
  Browser::Create(Browser::CreateParams(other_profile, /*user_gesture=*/true));
  SetTargetBrowser(CreateBrowser(browser()->profile()));
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu. If
// a new button is added to the menu, it should also be added to this list.
constexpr std::array kActionableItems_WebOnly = {
    ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kSigninAccountButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_WebOnly,
                        ProfileMenuClickTest_WebOnly) {
  // Add an account, not signed in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::AccountAvailabilityOptionsBuilder builder;
  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      builder.WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build(kTestEmail));
  ASSERT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_EQ(identity_manager->GetAccountsWithRefreshTokens().size(), 1u);

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu. If
// a new button is added to the menu, it should also be added to this list.
constexpr std::array kActionableItems_SyncEnabled = {
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton};

// TODO(crbug.com/341975308): re-enable test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ProfileMenuClickTest_SyncEnabled \
  DISABLED_ProfileMenuClickTest_SyncEnabled
#else
#define MAYBE_ProfileMenuClickTest_SyncEnabled ProfileMenuClickTest_SyncEnabled
#endif
PROFILE_MENU_CLICK_TEST(kActionableItems_SyncEnabled,
                        MAYBE_ProfileMenuClickTest_SyncEnabled) {
  EnableSync();
  RunTest();
}

// List of actionable items in the correct order as they appear in the menu with
// Sync error. If a new button is added to the menu, it should also be added to
// this list.
constexpr std::array kActionableItems_SyncError = {
    ProfileMenuViewBase::ActionableItem::kSyncErrorButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kSyncErrorButton};

PROFILE_MENU_CLICK_TEST(kActionableItems_SyncError,
                        ProfileMenuClickTest_SyncError) {
  ASSERT_TRUE(sync_harness()->SetupSyncWithCustomSettingsNoWaitForCompletion(
      /*user_settings_callback=*/base::BindOnce(
          [](syncer::SyncUserSettings* user_settings) {
            // Do not invoke SetInitialSyncFeatureSetupComplete(), meaning that
            // the user didn't confirm the sync settings.
          })));

  // Check that the setup was successful, but sync the feature is disabled
  // because SetInitialSyncFeatureSetupComplete() wasn't invoked.
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  ASSERT_FALSE(sync_service()->IsSyncFeatureEnabled());

  RunTest();
}

// List of actionable items in the correct order as they appear in the menu in
// Sync paused. If a new button is added to the menu, it should also be added to
// this list.
constexpr std::array kActionableItems_SyncPaused = {
    ProfileMenuViewBase::ActionableItem::kSyncErrorButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kSyncErrorButton};

// TODO(crbug.com/40822972): flaky on Windows and Mac
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

// List of actionable items in the correct order as they appear in the menu with
// signin disallowed. If a new button is added to the menu, it should also be
// added to this list.
constexpr std::array kActionableItems_SigninDisallowed = {
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton};

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

// List of actionable items in the correct order as they appear in the menu. If
// a new button is added to the menu, it should also be added to this list.
constexpr std::array kActionableItems_WithUnconsentedPrimaryAccount = {
    ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kSignoutButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kSigninAccountButton};

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

// List of actionable items in the correct order as they appear in the menu. If
// a new button is added to the menu, it should also be added to this list.
constexpr std::array kActionableItems_NewSyncPromoVariant = {
    ProfileMenuViewBase::ActionableItem::kHistorySyncOptInButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kSignoutButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kHistorySyncOptInButton};

const std::vector<base::test::FeatureRefAndParams>
    kNewSyncPromoVariantEnabledFeatures = {
        {switches::kEnableHistorySyncOptinExpansionPill,
         {{"history-sync-optin-expansion-pill-option",
           "browse-across-devices-new-profile-menu-promo-variant"}}}};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(kActionableItems_NewSyncPromoVariant,
                                     ProfileMenuClickTest_NewSyncPromoVariant,
                                     kNewSyncPromoVariantEnabledFeatures,
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

// List of actionable items in the correct order as they appear in the menu in
// signin pending state. If a new button is added to the menu, it should also be
// added to this list.
constexpr std::array kActionableItems_WithPendingAccount = {
    ProfileMenuViewBase::ActionableItem::kSigninReauthButton,
    ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
    ProfileMenuViewBase::ActionableItem::kEditProfileButton,
    ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
    ProfileMenuViewBase::ActionableItem::kSignoutButton,
    ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
    ProfileMenuViewBase::ActionableItem::kGuestProfileButton,
    ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
    // The first button is added again to finish the cycle and test that
    // there are no other buttons at the end.
    ProfileMenuViewBase::ActionableItem::kSigninReauthButton};

// TODO(crbug.com/40822972): flaky on Windows and Mac
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ProfileMenuClickTest_WithPendingAccount \
  DISABLED_ProfileMenuClickTest_WithPendingAccount
#else
#define MAYBE_ProfileMenuClickTest_WithPendingAccount \
  ProfileMenuClickTest_WithPendingAccount
#endif
PROFILE_MENU_CLICK_TEST(kActionableItems_WithPendingAccount,
                        MAYBE_ProfileMenuClickTest_WithPendingAccount) {
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

constexpr std::array
    kActionableItems_GuestProfileButtonNotAvailable_SignedInSupervised = {
        ProfileMenuViewBase::ActionableItem::kProfileManagementLabel,
        ProfileMenuViewBase::ActionableItem::kSigninAccountButton,
        ProfileMenuViewBase::ActionableItem::kAutofillSettingsButton,
        ProfileMenuViewBase::ActionableItem::kManageGoogleAccountButton,
        ProfileMenuViewBase::ActionableItem::kEditProfileButton,
        ProfileMenuViewBase::ActionableItem::kSyncSettingsButton,
        ProfileMenuViewBase::ActionableItem::kSignoutButton,
        ProfileMenuViewBase::ActionableItem::kAddNewProfileButton,
        // The kGuestProfileButton entry is not present.
        ProfileMenuViewBase::ActionableItem::kManageProfilesButton,
        // The first button is added again to finish the cycle and test that
        // there are no other buttons at the end.
        ProfileMenuViewBase::ActionableItem::kProfileManagementLabel,
};

PROFILE_MENU_CLICK_WITH_FEATURE_TEST(
    kActionableItems_GuestProfileButtonNotAvailable_SignedInSupervised,
    ProfileMenuClickTest_GuestProfileButtonNotAvailable_SignedInSupervised,
    kManagedProfileEnabledFeatures,
    /*disabled_features=*/{}) {
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
constexpr std::array kActionableItems_IncognitoProfile = {
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
constexpr std::array kActionableItems_GuestProfile = {
    ProfileMenuViewBase::ActionableItem::kExitProfileButton,
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
constexpr std::array kActionableItems_PasswordManagerWebApp = {
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
constexpr std::array kActionableItems_RegularWebApp = {
    ProfileMenuViewBase::ActionableItem::kOtherProfileButton};
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

class ProfileMenuSigninAccessPointTest
    : public testing::WithParamInterface<bool>,
      public SigninBrowserTestBase {
 public:
  // SigninBrowserTestBase:
  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    // Add a signed in account.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    account_info_ = identity_test_env()->MakeAccountAvailable(
        kTestEmail,
        {.primary_account_consent_level = signin::ConsentLevel::kSignin,
         .set_cookie = true});
    ASSERT_TRUE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    ASSERT_EQ(identity_manager->GetAccountsWithRefreshTokens().size(), 1u);
  }

 protected:
  ProfileMenuSigninAccessPointTest()
      : delegate_auto_reset_(signin_ui_util::SetSigninUiDelegateForTesting(
            &mock_signin_ui_delegate_)) {
    if (IsNewPromoVariantEnabled()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          switches::kEnableHistorySyncOptinExpansionPill,
          {{"history-sync-optin-expansion-pill-option",
            "browse-across-devices-new-profile-menu-promo-variant"}});
    }
  }

  bool IsNewPromoVariantEnabled() const { return GetParam(); }

  void OpenProfileMenuFromCoordinator(
      std::optional<signin_metrics::AccessPoint> explicit_access_point =
          std::nullopt) {
    auto* coordinator =
        ProfileMenuCoordinator::GetOrCreateForBrowser(browser());
    ASSERT_TRUE(coordinator);
    coordinator->Show(/*is_source_accelerator=*/false, explicit_access_point);
    ASSERT_NO_FATAL_FAILURE(
        WaitForMenuToBeActive(coordinator->GetProfileMenuViewBaseForTesting()));
  }

  void ClickSyncButton() {
    auto* coordinator = ProfileMenuCoordinator::FromBrowser(browser());
    ASSERT_TRUE(coordinator);
    ProfileMenuViewBase* profile_menu_view =
        coordinator->GetProfileMenuViewBaseForTesting();
    ASSERT_TRUE(profile_menu_view);
    profile_menu_view->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
    views::View* focused_view =
        profile_menu_view->GetFocusManager()->GetFocusedView();
    ASSERT_TRUE(focused_view);
    Click(focused_view);
  }

  CoreAccountInfo account_info_;

  StrictMock<MockSigninUiDelegate> mock_signin_ui_delegate_;

 private:
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ProfileMenuSigninAccessPointTest,
                       DefaultSigninAccessPoint) {
  base::HistogramTester histogram_tester;
  const signin_metrics::AccessPoint default_access_point =
      signin_metrics::AccessPoint::kAvatarBubbleSignIn;
  ASSERT_NO_FATAL_FAILURE(OpenProfileMenuFromCoordinator());
  // `Signin.SyncOptIn.Offered` should be recorded if the sync opt-in is
  // offered from the profile menu.
  histogram_tester.ExpectUniqueSample("Signin.SyncOptIn.Offered",
                                      default_access_point,
                                      /*expected_bucket_count=*/1);
  // `Signin.SignIn.Offered` should NOT be recorded if the sign-in is not
  // directly offered from the profile menu.
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Offered",
                                      default_access_point,
                                      /*expected_bucket_count=*/0);
  EXPECT_CALL(
      mock_signin_ui_delegate_,
      ShowTurnSyncOnUI(browser()->profile(), default_access_point,
                       signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
                       account_info_.account_id,
                       TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
                       /*is_sync_promo=*/false,
                       /*turn_sync_on_signed_profile=*/true));
  ASSERT_NO_FATAL_FAILURE(ClickSyncButton());
  const ProfileMenuViewBase::ActionableItem actionable_item =
      IsNewPromoVariantEnabled()
          ? ProfileMenuViewBase::ActionableItem::kHistorySyncOptInButton
          : ProfileMenuViewBase::ActionableItem::kSigninAccountButton;
  histogram_tester.ExpectUniqueSample("Profile.Menu.ClickedActionableItem",
                                      actionable_item,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(ProfileMenuSigninAccessPointTest,
                       ExplicitSigninAccessPoint) {
  base::HistogramTester histogram_tester;
  const signin_metrics::AccessPoint explicit_access_point =
      signin_metrics::AccessPoint::kHistorySyncOptinExpansionPillOnStartup;
  ASSERT_NO_FATAL_FAILURE(
      OpenProfileMenuFromCoordinator(explicit_access_point));
  // `Signin.SyncOptIn.Offered` should be recorded if the sync opt-in is
  // offered from the profile menu.
  histogram_tester.ExpectUniqueSample("Signin.SyncOptIn.Offered",
                                      explicit_access_point,
                                      /*expected_bucket_count=*/1);
  // `Signin.SignIn.Offered` should NOT be recorded if the sign-in is not
  // directly offered from the profile menu.
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Offered",
                                      explicit_access_point,
                                      /*expected_bucket_count=*/0);
  EXPECT_CALL(
      mock_signin_ui_delegate_,
      ShowTurnSyncOnUI(browser()->profile(), explicit_access_point,
                       signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
                       account_info_.account_id,
                       TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
                       /*is_sync_promo=*/false,
                       /*turn_sync_on_signed_profile=*/true));
  ASSERT_NO_FATAL_FAILURE(ClickSyncButton());
  const ProfileMenuViewBase::ActionableItem actionable_item =
      GetParam() ? ProfileMenuViewBase::ActionableItem::kHistorySyncOptInButton
                 : ProfileMenuViewBase::ActionableItem::kSigninAccountButton;
  histogram_tester.ExpectUniqueSample("Profile.Menu.ClickedActionableItem",
                                      actionable_item,
                                      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileMenuSigninAccessPointTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NewPromoVariantEnabled"
                                             : "NewPromoVariantDisabled";
                         });
