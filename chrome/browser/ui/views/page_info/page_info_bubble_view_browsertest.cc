// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/browser/ui/views/page_info/security_information_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/page_info/core/about_this_site_validation.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/page_info/page_info.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_test_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/widget_test.h"

using AboutThisSiteStatus =
    page_info::about_this_site_validation::AboutThisSiteStatus;

namespace {

constexpr char kExpiredCertificateFile[] = "expired_cert.pem";

class ClickEvent : public ui::Event {
 public:
  ClickEvent() : ui::Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

void PerformMouseClickOnView(views::View* view) {
  ui::AXActionData data;
  data.action = ax::mojom::Action::kDoDefault;
  view->HandleAccessibleAction(data);
}

// Clicks the location icon to open the page info bubble.
void OpenPageInfoBubble(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  LocationIconView* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);
  ClickEvent event;
  location_icon_view->ShowBubble(event);
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_NE(nullptr, page_info);
  page_info->set_close_on_deactivate(false);
}

// Opens the Page Info bubble and retrieves the UI view identified by
// |view_id|.
views::View* GetView(Browser* browser, int view_id) {
  views::Widget* page_info_bubble =
      PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetWidget();
  EXPECT_TRUE(page_info_bubble);

  views::View* view = page_info_bubble->GetRootView()->GetViewByID(view_id);
  EXPECT_TRUE(view);
  return view;
}

// Clicks the "Site settings" button from Page Info and waits for a "Settings"
// tab to open.
void ClickAndWaitForSettingsPageToOpen(views::View* site_settings_button) {
  content::WebContentsAddedObserver new_tab_observer;
  PerformMouseClickOnView(site_settings_button);

  std::u16string expected_title(u"Settings");
  content::TitleWatcher title_watcher(new_tab_observer.GetWebContents(),
                                      expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Returns the URL of the new tab that's opened on clicking the "Site settings"
// button from Page Info.
const GURL OpenSiteSettingsForUrl(Browser* browser, const GURL& url) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  OpenPageInfoBubble(browser);
  // Get site settings button.
  views::View* site_settings_button = GetView(
      browser,
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS);
  ClickAndWaitForSettingsPageToOpen(site_settings_button);

  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetLastCommittedURL();
}

void AddHintForTesting(Browser* browser,
                       const GURL& url,
                       page_info::proto::SiteInfo site_info) {
  optimization_guide::OptimizationMetadata optimization_metadata;
  page_info::proto::AboutThisSiteMetadata metadata;
  *metadata.mutable_site_info() = site_info;
  optimization_metadata.SetAnyMetadataForTesting(metadata);

  auto* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser->profile());
  optimization_guide_decider->AddHintForTesting(
      url, optimization_guide::proto::ABOUT_THIS_SITE, optimization_metadata);
}

page_info::proto::SiteInfo CreateValidSiteInfo() {
  page_info::proto::SiteInfo site_info;
  auto* description = site_info.mutable_description();
  description->set_description(
      "A domain used in illustrative examples in documents");
  description->set_lang("en_US");
  description->set_name("Example");
  description->mutable_source()->set_url("https://example.com");
  description->mutable_source()->set_label("Example source");

  return site_info;
}

}  // namespace

class PageInfoBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  PageInfoBubbleViewBrowserTest() = default;
  PageInfoBubbleViewBrowserTest(const PageInfoBubbleViewBrowserTest& test) =
      delete;
  PageInfoBubbleViewBrowserTest& operator=(
      const PageInfoBubbleViewBrowserTest& test) = delete;

  void SetUp() override {
    MockCertificateViewerForTesting();
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  GURL GetSimplePageUrl() const {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("simple.html")));
  }

  GURL GetIframePageUrl() const {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("iframe_blank.html")));
  }

  void ExecuteJavaScriptForTests(const std::string& js) {
    base::RunLoop run_loop;
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(js),
        base::BindOnce(
            [](base::OnceClosure quit_callback, base::Value result) {
              std::move(quit_callback).Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  PageInfo* GetPresenter() const {
    PageInfo* presenter = nullptr;
    presenter = static_cast<PageInfoBubbleView*>(
                    PageInfoBubbleView::GetPageInfoBubbleForTesting())
                    ->presenter_for_testing();
    DCHECK(presenter);
    return presenter;
  }

  void SetPageInfoBubbleIdentityInfo(
      const PageInfoUI::IdentityInfo& identity_info) {
    auto* presenter = GetPresenter();
    EXPECT_TRUE(presenter->ui_for_testing());
    presenter->ui_for_testing()->SetIdentityInfo(identity_info);
  }

  std::u16string GetCertificateButtonTitle() const {
    // Only PageInfoBubbleViewBrowserTest can access certificate_button_ in
    // PageInfoBubbleView, or title() in HoverButton.
    auto* certificate_button = static_cast<PageInfoHoverButton*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetViewByID(
            PageInfoViewFactory::
                VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER));
    return certificate_button->title()->GetText();
  }

  std::u16string GetCertificateButtonSubtitle() const {
    auto* certificate_button = static_cast<PageInfoHoverButton*>(
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetViewByID(
            PageInfoViewFactory::
                VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER));
    return certificate_button->subtitle()->GetText();
  }

  const std::u16string GetPageInfoBubbleViewDetailText() {
    auto* label =
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetViewByID(
            PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_DETAILS_LABEL);
    return static_cast<views::StyledLabel*>(label)->GetText();
  }

  const std::u16string GetPageInfoBubbleViewSummaryText() {
    auto* label =
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetViewByID(
            PageInfoViewFactory::VIEW_ID_PAGE_INFO_SECURITY_SUMMARY_LABEL);
    return static_cast<views::StyledLabel*>(label)->GetText();
  }

  const std::u16string GetSecurityInformationButtonText() {
    auto* button =
        PageInfoBubbleView::GetPageInfoBubbleForTesting()->GetViewByID(
            PageInfoViewFactory::
                VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SECURITY_INFORMATION);
    return static_cast<PageInfoHoverButton*>(button)->title()->GetText();
  }

  void SetupSentimentServiceExpectations(bool interacted) {
    testing::InSequence sequence;
    EXPECT_CALL(*mock_sentiment_service_, PageInfoOpened);
    EXPECT_CALL(*mock_sentiment_service_, InteractedWithPageInfo)
        .Times(interacted ? testing::Exactly(1) : testing::Exactly(0));
    EXPECT_CALL(*mock_sentiment_service_, PageInfoClosed);
  }

  raw_ptr<MockTrustSafetySentimentService> mock_sentiment_service_;

 private:
  std::vector<PageInfoViewFactory::PageInfoViewID> expected_identifiers_;
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ShowBubble) {
  SetupSentimentServiceExpectations(/*interacted=*/false);
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       StopShowingBubbleWhenWebContentsDestroyed) {
  // Open a new tab so the whole browser does not close once we close
  // the tab via WebContents::Close() below.
  ASSERT_TRUE(AddTabAtIndex(0, GURL("data:text/html,<p>puppies!</p>"),
                            ui::PAGE_TRANSITION_TYPED));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());

  web_contents()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ChromeURL) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings")));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ChromeExtensionURL) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome-extension://extension-id/options.html")));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ChromeDevtoolsURL) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("devtools://devtools/bundled/inspector.html")));
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, ViewSourceURL) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  web_contents()->GetMainFrame()->ViewSource();
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

// Test opening "Site Details" via Page Info from an ASCII origin does the
// correct URL canonicalization.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, SiteSettingsLink) {
  SetupSentimentServiceExpectations(/*interacted=*/true);
  GURL url = GURL("https://www.google.com/");
  std::string expected_origin = "https%3A%2F%2Fwww.google.com";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url));
}

// Test opening "Site Details" via Page Info from a non-ASCII URL converts it to
// an origin and does punycode conversion as well as URL canonicalization.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkWithNonAsciiUrl) {
  GURL url = GURL("http://🥄.ws/other/stuff.htm");
  std::string expected_origin = "http%3A%2F%2Fxn--9q9h.ws";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url));
}

// Test opening "Site Details" via Page Info from an origin with a non-default
// (scheme, port) pair will specify port # in the origin passed to query params.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkWithNonDefaultPort) {
  GURL url = GURL("https://www.example.com:8372");
  std::string expected_origin = "https%3A%2F%2Fwww.example.com%3A8372";
  EXPECT_EQ(GURL(chrome::kChromeUISiteDetailsPrefixURL + expected_origin),
            OpenSiteSettingsForUrl(browser(), url));
}

// Test opening "Site Details" via Page Info from about:blank goes to "Content
// Settings" (the alternative is a blank origin being sent to "Site Details").
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       SiteSettingsLinkWithAboutBlankURL) {
  EXPECT_EQ(GURL(chrome::kChromeUIContentSettingsURL),
            OpenSiteSettingsForUrl(browser(), GURL(url::kAboutBlankURL)));
}

// Test opening page info bubble that matches
// SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE threat type.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       VerifyEnterprisePasswordReusePageInfoBubble) {
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/")));

  // Update security state of the current page to match
  // SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  safe_browsing::ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      safe_browsing::ReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
  service->set_reused_password_account_type_for_last_shown_warning(
      reused_password_account_type);

  scoped_refptr<safe_browsing::PasswordProtectionRequest> request =
      safe_browsing::CreateDummyRequest(web_contents());
  service->ShowModalWarning(
      request.get(),
      safe_browsing::LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", reused_password_account_type);

  OpenPageInfoBubble(browser());
  views::View* change_password_button = GetView(
      browser(), PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  views::View* allowlist_password_reuse_button = GetView(
      browser(),
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
            visible_security_state->malicious_content_status);
  ASSERT_EQ(l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE) +
                u" " + l10n_util::GetStringUTF16(IDS_LEARN_MORE),
            GetPageInfoBubbleViewDetailText());

  // Verify these two buttons are showing.
  EXPECT_TRUE(change_password_button->GetVisible());
  EXPECT_TRUE(allowlist_password_reuse_button->GetVisible());

  // Verify clicking on button will increment corresponding bucket
  // PasswordProtection.PageInfoAction.NonGaiaEnterprisePasswordEntry histogram.
  PerformMouseClickOnView(change_password_button);
  EXPECT_THAT(
      histograms.GetAllSamples(
          safe_browsing::kEnterprisePasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1)));

  PerformMouseClickOnView(allowlist_password_reuse_button);
  EXPECT_THAT(
      histograms.GetAllSamples(
          safe_browsing::kEnterprisePasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1),
          base::Bucket(static_cast<int>(
                           safe_browsing::WarningAction::MARK_AS_LEGITIMATE),
                       1)));
  // Security state will change after allowlisting.
  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            visible_security_state->malicious_content_status);
}

// Test opening page info bubble that matches
// SB_THREAT_TYPE_SAVED_PASSWORD_REUSE threat type.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       VerifySavedPasswordReusePageInfoBubble) {
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/")));

  // Update security state of the current page to match
  // SB_THREAT_TYPE_SAVED_PASSWORD_REUSE.
  safe_browsing::ChromePasswordProtectionService* service =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(browser()->profile());
  safe_browsing::ReusedPasswordAccountType reused_password_account_type;
  reused_password_account_type.set_account_type(
      safe_browsing::ReusedPasswordAccountType::SAVED_PASSWORD);
  service->set_reused_password_account_type_for_last_shown_warning(
      reused_password_account_type);

  scoped_refptr<safe_browsing::PasswordProtectionRequest> request =
      safe_browsing::CreateDummyRequest(web_contents());
  service->ShowModalWarning(
      request.get(),
      safe_browsing::LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED,
      "unused_token", reused_password_account_type);

  OpenPageInfoBubble(browser());
  views::View* change_password_button = GetView(
      browser(), PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  views::View* allowlist_password_reuse_button = GetView(
      browser(),
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      helper->GetVisibleSecurityState();
  ASSERT_EQ(security_state::MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE,
            visible_security_state->malicious_content_status);

  // Verify these two buttons are showing.
  EXPECT_TRUE(change_password_button->GetVisible());
  EXPECT_TRUE(allowlist_password_reuse_button->GetVisible());

  // Verify clicking on each button will both inform the sentiment service,
  // and increment the corresponding bucket of
  // PasswordProtection.PageInfoAction.NonGaiaEnterprisePasswordEntry
  // histogram.
  EXPECT_CALL(*mock_sentiment_service_, InteractedWithPageInfo).Times(2);

  PerformMouseClickOnView(change_password_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSavedPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1)));

  PerformMouseClickOnView(allowlist_password_reuse_button);
  EXPECT_THAT(
      histograms.GetAllSamples(safe_browsing::kSavedPasswordPageInfoHistogram),
      testing::ElementsAre(
          base::Bucket(static_cast<int>(safe_browsing::WarningAction::SHOWN),
                       1),
          base::Bucket(
              static_cast<int>(safe_browsing::WarningAction::CHANGE_PASSWORD),
              1),
          base::Bucket(static_cast<int>(
                           safe_browsing::WarningAction::MARK_AS_LEGITIMATE),
                       1)));
  // Security state will change after allowlisting.
  visible_security_state = helper->GetVisibleSecurityState();
  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_NONE,
            visible_security_state->malicious_content_status);
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       ClosesOnUserNavigateToSamePage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimplePageUrl()));
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimplePageUrl()));
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       ClosesOnUserNavigateToDifferentPage) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetSimplePageUrl()));
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetIframePageUrl()));
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       DoesntCloseOnSubframeNavigate) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetIframePageUrl()));
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
  content::NavigateIframeToURL(web_contents(), "test", GetSimplePageUrl());
  // Expect that the bubble is still open even after a subframe navigation has
  // happened.
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleView::GetShownBubbleType());
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest,
                       InteractedWithCookiesButton) {
  SetupSentimentServiceExpectations(/*interacted=*/true);

  base::RunLoop run_loop;
  GetPageInfoDialogCreatedCallbackForTesting() = run_loop.QuitClosure();
  OpenPageInfoBubble(browser());
  run_loop.Run();

  views::View* cookies_button = GetView(
      browser(),
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG);
  PerformMouseClickOnView(cookies_button);
}

class PageInfoBubbleViewBrowserTestWithAutoupgradesDisabled
    : public PageInfoBubbleViewBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PageInfoBubbleViewBrowserTest::SetUpCommandLine(command_line);
    feature_list.InitAndDisableFeature(
        blink::features::kMixedContentAutoupgrade);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

// Ensure changes to security state are reflected in an open PageInfo bubble.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTestWithAutoupgradesDisabled,
                       UpdatesOnSecurityStateChange) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/delayed_mixed_content.html")));
  OpenPageInfoBubble(browser());

  EXPECT_EQ(GetSecurityInformationButtonText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURE_SUMMARY));

  ExecuteJavaScriptForTests("load_mixed();");
  EXPECT_EQ(GetPageInfoBubbleViewSummaryText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_MIXED_CONTENT_SUMMARY));
}

// Ensure a page can both have an invalid certificate *and* be blocked by Safe
// Browsing.  Regression test for bug 869925.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, BlockedAndInvalidCert) {
  SetupSentimentServiceExpectations(/*interacted=*/true);

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/simple.html")));

  // Setup the bogus identity with an expired cert and SB flagging.
  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_ERROR;
  identity.certificate = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                                 kExpiredCertificateFile);
  identity.safe_browsing_status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;
  OpenPageInfoBubble(browser());

  SetPageInfoBubbleIdentityInfo(identity);

  // Verify bubble complains of malware...
  EXPECT_EQ(GetPageInfoBubbleViewSummaryText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_MALWARE_SUMMARY));

  // ...and has a "Certificate (Invalid)" button.
  std::u16string invalid_text;
  invalid_text =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CERTIFICATE_IS_NOT_VALID);
  EXPECT_EQ(GetCertificateButtonTitle(), invalid_text);

  // Check that clicking the certificate viewer button is reported to the
  // sentiment service.
  views::View* certificates_button = GetView(
      browser(),
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER);
  PerformMouseClickOnView(certificates_button);
}

// Ensure a page that has an EV certificate *and* is blocked by Safe Browsing
// shows the correct PageInfo UI. Regression test for crbug.com/1014240.
IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewBrowserTest, MalwareAndEvCert) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/simple.html")));

  // Generate a valid mock EV HTTPS identity, with an EV certificate. Must
  // match conditions in PageInfoBubbleView::SetIdentityInfo() for setting
  // the certificate button subtitle.
  PageInfoUI::IdentityInfo identity;
  identity.identity_status = PageInfo::SITE_IDENTITY_STATUS_EV_CERT;
  identity.connection_status = PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED;
  scoped_refptr<net::X509Certificate> ev_cert =
      net::X509Certificate::CreateFromBytes(thawte_der);
  ASSERT_TRUE(ev_cert);
  identity.certificate = ev_cert;

  // Have the page also trigger an SB malware warning.
  identity.safe_browsing_status = PageInfo::SAFE_BROWSING_STATUS_MALWARE;

  OpenPageInfoBubble(browser());
  SetPageInfoBubbleIdentityInfo(identity);

  // Verify bubble complains of malware...
  EXPECT_EQ(GetPageInfoBubbleViewSummaryText(),
            l10n_util::GetStringUTF16(IDS_PAGE_INFO_MALWARE_SUMMARY));

  // ...and has the correct organization details in the Certificate button.
  EXPECT_EQ(GetCertificateButtonSubtitle(),
            l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
                u"Thawte Inc", u"US"));
}

class PageInfoBubbleViewPrerenderBrowserTest
    : public PageInfoBubbleViewBrowserTest {
 public:
  PageInfoBubbleViewPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&PageInfoBubbleViewBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~PageInfoBubbleViewPrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    PageInfoBubbleViewBrowserTest::SetUp();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewPrerenderBrowserTest,
                       DoesntCloseOnPrerenderNavigate) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html")));
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
  OpenPageInfoBubble(browser());
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());
  // Start a prerender.
  prerender_helper_.AddPrerender(
      embedded_test_server()->GetURL("/title1.html"));
  // Ensure the bubble is still open after prerender navigation.
  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());
}

class PageInfoBubbleViewAboutThisSiteBrowserTest : public InProcessBrowserTest {
 public:
  PageInfoBubbleViewAboutThisSiteBrowserTest() {
    feature_list.InitAndEnableFeature(page_info::kPageInfoAboutThisSite);
  }

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(optimization_guide::switches::
                          kDisableCheckingUserPermissionsForTesting);
  }

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewAboutThisSiteBrowserTest,
                       AboutThisSite) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histograms;

  auto url = https_server_.GetURL("a.test", "/title1.html");
  AddHintForTesting(browser(), url, CreateValidSiteInfo());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  OpenPageInfoBubble(browser());

  auto* page_info = PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_TRUE(page_info->GetViewByID(
      PageInfoViewFactory::
          VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SECURITY_INFORMATION));
  EXPECT_TRUE(page_info->GetViewByID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_ABOUT_THIS_SITE_BUTTON));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AboutThisSiteStatus::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries[0], url);
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::AboutThisSiteStatus::kStatusName,
      static_cast<int>(AboutThisSiteStatus::kValid));

  page_info->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  base::RunLoop().RunUntilIdle();
  histograms.ExpectTotalCount("Security.PageInfo.TimeOpen.AboutThisSiteShown",
                              1);
  histograms.ExpectTotalCount(
      "Security.PageInfo.TimeOpen.AboutThisSiteNotShown", 0);
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewAboutThisSiteBrowserTest,
                       AboutThisSiteNotValid) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histograms;

  auto url = https_server_.GetURL("a.test", "/title1.html");
  page_info::proto::SiteInfo site_info;
  // Incomplete description, missing source, name and lang.
  auto* description = site_info.mutable_description();
  description->set_description(
      "A domain used in illustrative examples in documents");
  AddHintForTesting(browser(), url, site_info);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  OpenPageInfoBubble(browser());

  auto* page_info = PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_TRUE(page_info->GetViewByID(
      PageInfoViewFactory::
          VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SECURITY_INFORMATION));
  EXPECT_FALSE(page_info->GetViewByID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_ABOUT_THIS_SITE_BUTTON));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AboutThisSiteStatus::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries[0], url);
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::AboutThisSiteStatus::kStatusName,
      static_cast<int>(AboutThisSiteStatus::kMissingDescriptionName));

  page_info->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  base::RunLoop().RunUntilIdle();
  histograms.ExpectTotalCount("Security.PageInfo.TimeOpen.AboutThisSiteShown",
                              0);
  histograms.ExpectTotalCount(
      "Security.PageInfo.TimeOpen.AboutThisSiteNotShown", 1);
}

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewAboutThisSiteBrowserTest,
                       AboutThisSiteNotSecure) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histograms;
  ASSERT_TRUE(embedded_test_server()->Start());

  auto url = embedded_test_server()->GetURL("a.test", "/title1.html");
  AddHintForTesting(browser(), url, CreateValidSiteInfo());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  OpenPageInfoBubble(browser());
  auto* page_info = PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_FALSE(page_info->GetViewByID(
      PageInfoViewFactory::
          VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SECURITY_INFORMATION));
  // The button isn't shown because connection isn't secure...
  EXPECT_FALSE(page_info->GetViewByID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_ABOUT_THIS_SITE_BUTTON));

  // ...and no UKM is recoreded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AboutThisSiteStatus::kEntryName);
  EXPECT_EQ(0u, entries.size());

  page_info->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  base::RunLoop().RunUntilIdle();
  histograms.ExpectTotalCount("Security.PageInfo.TimeOpen.AboutThisSiteShown",
                              0);
  histograms.ExpectTotalCount(
      "Security.PageInfo.TimeOpen.AboutThisSiteNotShown", 1);
}

// Test that no info is shown and "kUnknown" is logged when hints fetching is
// disabled.
class PageInfoBubbleViewAboutThisSiteDisabledBrowserTest
    : public PageInfoBubbleViewAboutThisSiteBrowserTest {
  void SetUpCommandLine(base::CommandLine* cmd) override {
    // Don't set the flag to enable hints fetching.
  }
};

IN_PROC_BROWSER_TEST_F(PageInfoBubbleViewAboutThisSiteDisabledBrowserTest,
                       AboutThisSiteWithoutOptin) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  auto url = https_server_.GetURL("a.test", "/title1.html");
  AddHintForTesting(browser(), url, CreateValidSiteInfo());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  OpenPageInfoBubble(browser());

  auto* page_info = PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_TRUE(page_info->GetViewByID(
      PageInfoViewFactory::
          VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SECURITY_INFORMATION));
  EXPECT_FALSE(page_info->GetViewByID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_ABOUT_THIS_SITE_BUTTON));

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AboutThisSiteStatus::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries[0], url);
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::AboutThisSiteStatus::kStatusName,
      static_cast<int>(AboutThisSiteStatus::kUnknown));
}
