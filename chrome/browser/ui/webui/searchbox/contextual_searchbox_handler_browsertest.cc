// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/pref_names.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display_switches.h"
#include "ui/views/interaction/element_tracker_views.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_signin_button_view.h"
#include "chrome/browser/ui/views/search_ai_mode/signin_promo_view.h"
#include "chrome/common/url_constants.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"
#endif

class TestSearchboxHandler : public ContextualSearchboxHandler {
 public:
  TestSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_page,
      Profile* profile,
      content::WebContents* web_contents,
      GetSessionHandleCallback get_session_callback)
      : ContextualSearchboxHandler(std::move(pending_page_handler),
                                   std::move(pending_page),
                                   profile,
                                   web_contents,
                                   std::make_unique<TestOmniboxClient>(),
                                   std::move(get_session_callback)) {}

  ~TestSearchboxHandler() override = default;

  void OnThumbnailRemoved() override {}
};

class ContextualSearchboxHandlerBrowserTest : public InProcessBrowserTest {
 public:
  ContextualSearchboxHandlerBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasksContext,
          {{"ContextualTasksContextSmartTabSharing", "true"}}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockSearchboxPage> page_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  std::unique_ptr<TestSearchboxHandler> handler_;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto* service =
        ContextualSearchServiceFactory::GetForProfile(browser()->GetProfile());
    session_handle_ = service->CreateSession(
        ntp_composebox::CreateQueryControllerConfigParams(),
        contextual_search::ContextualSearchSource::kUnknown,
        /*invocation_source=*/std::nullopt);
    // Check the search content sharing settings to notify the session handle
    // that the client is properly checking the pref value.
    session_handle_->CheckSearchContentSharingSettings(
        browser()->GetProfile()->GetPrefs());

    handler_ = std::make_unique<TestSearchboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        page_.BindAndGetRemote(), browser()->GetProfile(),
        /*web_contents=*/browser()->GetTabStripModel()->GetActiveWebContents(),
        base::BindLambdaForTesting([&]() { return session_handle_.get(); }));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForceDeviceScaleFactor);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");
  }

  void TearDownOnMainThread() override { handler_.reset(); }
};

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTest,
                       CreateTabPreviewEncodingOptions_NotScaled) {
  // When no device scale factor is applied, physical pixels should translate to
  // CSS pixels at a 1:1 ratio.
  int expected_width = 125 * 1;
  int expected_height = 200 * 1;

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto options = handler_->CreateTabPreviewEncodingOptions(web_contents);

  ASSERT_TRUE(options.has_value());
  EXPECT_EQ(options->max_width, expected_width);
  EXPECT_EQ(options->max_height, expected_height);
}

class ContextualSearchboxHandlerBrowserTestDSF2
    : public ContextualSearchboxHandlerBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContextualSearchboxHandlerBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kForceDeviceScaleFactor);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTestDSF2,
                       CreateTabPreviewEncodingOptions_Scaled) {
  // 60 physical pixels translates to 30 CSS pixels when the device scale factor
  // = 2 (2 physical pixels : 1 CSS pixel);
  int expected_width = 125 * 2;
  int expected_height = 200 * 2;

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto options = handler_->CreateTabPreviewEncodingOptions(web_contents);

  ASSERT_TRUE(options.has_value());
  EXPECT_EQ(options->max_width, expected_width);
  EXPECT_EQ(options->max_height, expected_height);
}

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTest,
                       ResetInputStateModel) {
  // Access private member via friend class.
  ContextualSearchboxHandler* base_handler = handler_.get();
  ASSERT_TRUE(base_handler->input_state_model_);

  handler_->ResetInputStateModel();

  EXPECT_FALSE(base_handler->input_state_model_);
}

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTest,
                       WaitForTabFaviconLoad_Async) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(tab);
  int32_t tab_id = tab->GetHandle().raw_value();

  base::test::TestFuture<const std::optional<GURL>&> future;
  handler_->WaitForTabFaviconLoad(tab_id, future.GetCallback());
  std::optional<GURL> data_url = future.Get();
  ASSERT_TRUE(data_url.has_value());
  EXPECT_TRUE(data_url->spec().starts_with("data:image/png;base64,"));
}

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTest,
                       WaitForTabFaviconLoad_Cached) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_favicon.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(tab);
  int32_t tab_id = tab->GetHandle().raw_value();

  // Ensure the favicon is fully processed and valid in the browser.
  {
    base::test::TestFuture<const std::optional<GURL>&> future;
    handler_->WaitForTabFaviconLoad(tab_id, future.GetCallback());
    ASSERT_TRUE(future.Get().has_value());
  }

  // The cached favicon data URL should be returned immediately.
  {
    base::test::TestFuture<const std::optional<GURL>&> future;
    handler_->WaitForTabFaviconLoad(tab_id, future.GetCallback());
    std::optional<GURL> data_url = future.Get();
    ASSERT_TRUE(data_url.has_value());
    EXPECT_TRUE(data_url->spec().starts_with("data:image/png;base64,"));
  }
}

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTest,
                       WaitForTabFaviconLoad_WebContentsDestroyed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/empty.html");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(tab);
  int32_t tab_id = tab->GetHandle().raw_value();

  base::test::TestFuture<const std::optional<GURL>&> future;
  handler_->WaitForTabFaviconLoad(tab_id, future.GetCallback());

  // Destroy the WebContents by closing the tab.
  browser()->GetTabStripModel()->CloseSelectedTabs();

  std::optional<GURL> data_url = future.Get();
  EXPECT_FALSE(data_url.has_value());
}

IN_PROC_BROWSER_TEST_F(ContextualSearchboxHandlerBrowserTest,
                       SmartTabSharingActive) {
  base::test::TestFuture<bool> get_future;
  handler_->GetSmartTabSharingActive(get_future.GetCallback());
  EXPECT_FALSE(get_future.Get());

  handler_->SetSmartTabSharingActive(true);

  base::test::TestFuture<bool> get_future2;
  handler_->GetSmartTabSharingActive(get_future2.GetCallback());
  EXPECT_TRUE(get_future2.Get());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ContextualSearchboxHandlerDriveSigninPromoBrowserTest
    : public ContextualSearchboxHandlerBrowserTest {
 public:
  ContextualSearchboxHandlerDriveSigninPromoBrowserTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasksContext,
          {{"ContextualTasksContextSmartTabSharing", "true"}}},
         {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
         {omnibox::kComposeboxDriveContextMenuOption, {}},
         {omnibox::kComposeboxDriveContextMenuOptionSigninPromo, {}},
         {switches::kEnableSearchAIModeSigninPromo, {}}},
        {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContextualSearchboxHandlerBrowserTest::SetUpInProcessBrowserTestFixture();
    url_loader_factory_helper_.SetUp();
  }

 protected:
  void TriggerDriveSigninPromoAndClickSignInButton() {
    ui::ElementContext context =
        BrowserView::GetBrowserViewForBrowser(browser())->GetElementContext();
    base::RunLoop button_ready_loop;
    auto subscription =
        ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
            kBubbleSignInPromoSignInButtonHasCallback, context,
            base::BindLambdaForTesting(
                [&](ui::TrackedElement*) { button_ready_loop.Quit(); }));

    base::test::TestFuture<searchbox::mojom::DriveUploadResponsePtr> future;
    handler_->OnDriveUploadClicked(future.GetCallback());
    EXPECT_TRUE(future.Wait());

    auto* promo_view = views::AsViewClass<ComposeboxDriveSignInPromoView>(
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kComposeboxDriveSignInPromoViewId, context));
    ASSERT_NE(promo_view, nullptr);
    promo_view->set_close_on_deactivate(false);
    button_ready_loop.Run();

    auto* sign_in_button = views::AsViewClass<views::Button>(
        promo_view->GetInitiallyFocusedView());
    ASSERT_NE(sign_in_button, nullptr);

    ui::MouseEvent press_event(
        ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
        base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    views::test::ButtonTestApi(sign_in_button).NotifyClick(press_event);
  }

  ChromeSigninClientWithURLLoaderHelper url_loader_factory_helper_;
};

IN_PROC_BROWSER_TEST_F(
    ContextualSearchboxHandlerDriveSigninPromoBrowserTest,
    OnDriveUploadClicked_SignedOut_ShowsComposeboxDriveSigninPromo) {
  // Enable context sharing in prefs.
  browser()->GetProfile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  base::test::TestFuture<searchbox::mojom::DriveUploadResponsePtr> future;
  handler_->OnDriveUploadClicked(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  views::View* promo_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kComposeboxDriveSignInPromoViewId,
          BrowserView::GetBrowserViewForBrowser(browser())
              ->GetElementContext());
  EXPECT_NE(promo_view, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    ContextualSearchboxHandlerDriveSigninPromoBrowserTest,
    OnDriveUploadClicked_SigninPending_ShowsComposeboxDriveSigninPromo) {
  // Enable context sharing in prefs.
  browser()->GetProfile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  // Set up primary account in persistent error state (Signin Pending).
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@gmail.com", signin::ConsentLevel::kSignin);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  base::test::TestFuture<searchbox::mojom::DriveUploadResponsePtr> future;
  handler_->OnDriveUploadClicked(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  views::View* promo_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kComposeboxDriveSignInPromoViewId,
          BrowserView::GetBrowserViewForBrowser(browser())
              ->GetElementContext());
  EXPECT_NE(promo_view, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    ContextualSearchboxHandlerDriveSigninPromoBrowserTest,
    OnDriveUploadClicked_FromOmniboxPopup_ClosesPopupAndShowsPromo) {
  // Enable context sharing in prefs.
  browser()->GetProfile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  // Create a popup WebContents with OmniboxPopupWebContentsHelper attached,
  // simulating the AIM / Full WebUI Omnibox popup.
  std::unique_ptr<content::WebContents> popup_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));
  webui::SetBrowserWindowInterface(popup_web_contents.get(), browser());
  OmniboxPopupWebContentsHelper::CreateForWebContents(popup_web_contents.get());
  OmniboxController* omnibox_controller = BrowserWindow::FromBrowser(browser())
                                              ->GetLocationBar()
                                              ->GetOmniboxController();
  OmniboxPopupWebContentsHelper::FromWebContents(popup_web_contents.get())
      ->set_omnibox_controller(omnibox_controller);

  omnibox_controller->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);
  ASSERT_EQ(omnibox_controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kAim);

  testing::NiceMock<MockSearchboxPage> popup_page;
  auto popup_handler = std::make_unique<TestSearchboxHandler>(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      popup_page.BindAndGetRemote(), browser()->GetProfile(),
      popup_web_contents.get(),
      base::BindLambdaForTesting([&]() { return session_handle_.get(); }));

  base::test::TestFuture<searchbox::mojom::DriveUploadResponsePtr> future;
  popup_handler->OnDriveUploadClicked(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  // The omnibox popup should be closed so it doesn't cover or steal activation
  // from the Drive sign-in promo bubble.
  EXPECT_EQ(omnibox_controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kNone);

  views::View* promo_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kComposeboxDriveSignInPromoViewId,
          BrowserView::GetBrowserViewForBrowser(browser())
              ->GetElementContext());
  EXPECT_NE(promo_view, nullptr);
}

IN_PROC_BROWSER_TEST_F(
    ContextualSearchboxHandlerDriveSigninPromoBrowserTest,
    OnDriveUploadClicked_OnNtp_ReusesSingleTabAndRedirectsToNtp) {
  browser()->GetProfile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           chrome::ChromeUINewTabURLAsGURL()));
  ASSERT_EQ(1, browser()->GetTabStripModel()->count());

  TriggerDriveSigninPromoAndClickSignInButton();

  // Clicking "Sign in to Chrome" on an NTP overwrites the existing NTP tab
  // in-place (ShowTabOverwritingNTP) rather than opening a 2nd tab.
  EXPECT_EQ(1, browser()->GetTabStripModel()->count());

  // Simulate completing sign-in from the promo access point. The single tab
  // redirects back to chrome://newtab/.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::
                               kComposeboxDriveContextMenuOptionBubble)
          .Build("user@gmail.com"));

  EXPECT_EQ(1, browser()->GetTabStripModel()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->GetTabStripModel()->GetActiveWebContents()));
  EXPECT_EQ(
      chrome::ChromeUINewTabURLAsGURL(),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    ContextualSearchboxHandlerDriveSigninPromoBrowserTest,
    OnDriveUploadClicked_OnWebpage_OpensNewTabWithNtpRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  browser()->GetProfile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  // Open a foreground webpage tab so the active tab WebContents differs from
  // `handler_`'s initial WebContents (which is now hidden, similar to the
  // Omnibox popup WebContents when triggered from a webpage).
  const GURL webpage_url = embedded_test_server()->GetURL("/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), webpage_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());

  TriggerDriveSigninPromoAndClickSignInButton();

  // Clicking the promo button from a non-NTP webpage opens a dedicated sign-in
  // tab, which ComposeboxDriveSignInPromoDelegate redirects to chrome://newtab/
  // once Gaia sign-in completes (even after navigating past the initial
  // signin_url, which sets DiceTabHelper::IsChromeSigninPage() to false).
  EXPECT_EQ(3, browser()->GetTabStripModel()->count());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::
                               kComposeboxDriveContextMenuOptionBubble)
          .Build("user@gmail.com"));

  EXPECT_EQ(
      chrome::ChromeUINewTabURLAsGURL(),
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(
    ContextualSearchboxHandlerDriveSigninPromoBrowserTest,
    OnDriveUploadClicked_WebOnlySignedInOnWebpage_StaysOnWebpage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  browser()->GetProfile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kEnabled));

  // Set up an account signed into the web only (no Chrome primary account).
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->GetProfile());
  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder(
          url_loader_factory_helper_.test_url_loader_factory())
          .WithCookie()
          .Build("webonly@gmail.com"));
  signin::SimulateSuccessfulFetchOfAccountInfo(
      identity_manager, account_info.GetAccountId(),
      std::string(account_info.GetEmail()), account_info.GetGaiaId(),
      /*hosted_domain=*/"", "Full Name", "Given", "en-US",
      /*picture_url=*/"");
  ASSERT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  const GURL webpage_url = embedded_test_server()->GetURL("/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), webpage_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());

  TriggerDriveSigninPromoAndClickSignInButton();

  // Clicking "Continue as <Name>" signs the user into Chrome without opening a
  // new tab and stays on the current webpage.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_EQ(2, browser()->GetTabStripModel()->count());
  EXPECT_EQ(
      webpage_url,
      browser()->GetTabStripModel()->GetActiveWebContents()->GetVisibleURL());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class WebuiOmniboxHandlerBrowserTest
    : public ContextualSearchboxHandlerBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ContextualSearchboxHandlerBrowserTest::SetUpOnMainThread();

    test_web_ui_.set_web_contents(
        browser()->GetTabStripModel()->GetActiveWebContents());

    omnibox_controller_ = std::make_unique<OmniboxController>(
        std::make_unique<TestOmniboxClient>());
    omnibox_controller_->SetEditModelForTesting(
        std::make_unique<testing::NiceMock<MockOmniboxEditModel>>(
            omnibox_controller_.get()));

    omnibox_handler_ = std::make_unique<WebuiOmniboxHandler>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        omnibox_page_.BindAndGetRemote(),
        /*metrics_reporter=*/nullptr, omnibox_controller_.get(), &test_web_ui_,
        base::BindLambdaForTesting([&]() { return session_handle_.get(); }));
  }

  void TearDownOnMainThread() override {
    omnibox_handler_.reset();
    omnibox_controller_.reset();
    ContextualSearchboxHandlerBrowserTest::TearDownOnMainThread();
  }

  content::TestWebUI test_web_ui_;
  testing::NiceMock<MockSearchboxPage> omnibox_page_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  std::unique_ptr<WebuiOmniboxHandler> omnibox_handler_;
};

IN_PROC_BROWSER_TEST_F(WebuiOmniboxHandlerBrowserTest,
                       AddTabContextRejectsTabFromDifferentProfile) {
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  tabs::TabInterface* incognito_tab =
      incognito_browser->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(incognito_tab);
  ASSERT_NE(incognito_tab->GetProfile(), browser()->GetProfile());

  base::test::TestFuture<base::expected<
      base::UnguessableToken, contextual_search::ContextUploadErrorType>>
      future;
  omnibox_handler_->AddTabContext(incognito_tab->GetHandle().raw_value(),
                                  /*delay_upload=*/false,
                                  searchbox::mojom::TabAttachmentSource::kOther,
                                  future.GetCallback());

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            contextual_search::ContextUploadErrorType::kBrowserProcessingError);
}

IN_PROC_BROWSER_TEST_F(WebuiOmniboxHandlerBrowserTest,
                       AddTabContextRejectsWhenContentSharingDisabled) {
  browser()->GetProfile()->GetPrefs()->SetInteger(
      contextual_search::kSearchContentSharingSettings,
      static_cast<int>(
          contextual_search::SearchContentSharingSettingsValue::kDisabled));

  tabs::TabInterface* tab = browser()->GetTabStripModel()->GetActiveTab();
  ASSERT_TRUE(tab);

  base::test::TestFuture<base::expected<
      base::UnguessableToken, contextual_search::ContextUploadErrorType>>
      future;
  omnibox_handler_->AddTabContext(tab->GetHandle().raw_value(),
                                  /*delay_upload=*/false,
                                  searchbox::mojom::TabAttachmentSource::kOther,
                                  future.GetCallback());

  auto result = future.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            contextual_search::ContextUploadErrorType::kBrowserProcessingError);
}
