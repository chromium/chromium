// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/security_state/core/security_state.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/cert/ct_policy_status.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/test/material_design_controller_test_api.h"

class LocationBarViewBrowserTest : public InProcessBrowserTest {
 public:
  LocationBarViewBrowserTest() = default;

  LocationBarView* GetLocationBarView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView();
  }

  PageActionIconView* GetZoomView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kZoom);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LocationBarViewBrowserTest);
};

// Ensure the location bar decoration is added when zooming, and is removed when
// the bubble is closed, but only if zoom was reset.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, LocationBarDecoration) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  PageActionIconView* zoom_view = GetZoomView();

  ASSERT_TRUE(zoom_view);
  EXPECT_FALSE(zoom_view->GetVisible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Altering zoom should display a bubble. Note ZoomBubbleView closes
  // asynchronously, so precede checks with a run loop flush.
  zoom_controller->SetZoomLevel(blink::PageZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Close the bubble at other than 100% zoom. Icon should remain visible.
  ZoomBubbleView::CloseCurrentBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Show the bubble again.
  zoom_controller->SetZoomLevel(blink::PageZoomFactorToZoomLevel(2.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Remains visible at 100% until the bubble is closed.
  zoom_controller->SetZoomLevel(blink::PageZoomFactorToZoomLevel(1.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Closing at 100% hides the icon.
  ZoomBubbleView::CloseCurrentBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(zoom_view->GetVisible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
}

// Ensure that location bar bubbles close when the webcontents hides.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, BubblesCloseOnHide) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  PageActionIconView* zoom_view = GetZoomView();

  ASSERT_TRUE(zoom_view);
  EXPECT_FALSE(zoom_view->GetVisible());

  zoom_controller->SetZoomLevel(blink::PageZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  chrome::NewTab(browser());
  chrome::SelectNextTab(browser());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
}

class TouchLocationBarViewBrowserTest : public LocationBarViewBrowserTest {
 public:
  TouchLocationBarViewBrowserTest() : test_api_(true) {}

 private:
  ui::test::MaterialDesignControllerTestAPI test_api_;
  DISALLOW_COPY_AND_ASSIGN(TouchLocationBarViewBrowserTest);
};

// Test the corners of the OmniboxViewViews do not get drawn on top of the
// rounded corners of the omnibox in touch mode.
IN_PROC_BROWSER_TEST_F(TouchLocationBarViewBrowserTest, OmniboxViewViewsSize) {
  // Make sure all the LocationBarView children are invisible. This should
  // ensure there are no trailing decorations at the end of the omnibox
  // (currently, the LocationIconView is *always* added as a leading decoration,
  // so it's not possible to test the leading side).
  views::View* omnibox_view_views = GetLocationBarView()->omnibox_view();
  for (views::View* child : GetLocationBarView()->children()) {
    if (child != omnibox_view_views)
      child->SetVisible(false);
  }

  GetLocationBarView()->Layout();
  // Check |omnibox_view_views| is not wider than the LocationBarView with its
  // rounded ends removed.
  EXPECT_LE(omnibox_view_views->width(),
            GetLocationBarView()->width() - GetLocationBarView()->height());
  // Check the trailing edge of |omnibox_view_views| does not exceed the
  // trailing edge of the LocationBarView with its endcap removed.
  EXPECT_LE(omnibox_view_views->bounds().right(),
            GetLocationBarView()->GetLocalBoundsWithoutEndcaps().right());
}

// Make sure the IME autocomplete selection text is positioned correctly when
// there are no trailing decorations.
IN_PROC_BROWSER_TEST_F(TouchLocationBarViewBrowserTest,
                       IMEInlineAutocompletePosition) {
  // Make sure all the LocationBarView children are invisible. This should
  // ensure there are no trailing decorations at the end of the omnibox.
  OmniboxViewViews* omnibox_view_views = GetLocationBarView()->omnibox_view();
  views::Label* ime_inline_autocomplete_view =
      GetLocationBarView()->ime_inline_autocomplete_view_;
  for (views::View* child : GetLocationBarView()->children()) {
    if (child != omnibox_view_views)
      child->SetVisible(false);
  }
  omnibox_view_views->SetText(base::UTF8ToUTF16("谷"));
  GetLocationBarView()->SetImeInlineAutocompletion(base::UTF8ToUTF16("歌"));
  EXPECT_TRUE(ime_inline_autocomplete_view->GetVisible());

  GetLocationBarView()->Layout();

  // Make sure the IME inline autocomplete view starts at the end of
  // |omnibox_view_views|.
  EXPECT_EQ(omnibox_view_views->bounds().right(),
            ime_inline_autocomplete_view->x());
}

// After SetUpInterceptor() is called, requests to this hostname will be mocked
// and use specified certificate validation results. This allows tests to mock
// Extended Validation (EV) certificate connections.
const char kMockSecureHostname[] = "example-secure.test";

struct SecurityIndicatorTestParams {
  bool is_enabled;
  bool use_secure_url;
  net::CertStatus cert_status;
  security_state::SecurityLevel security_level;
  bool should_show_text;
  base::string16 indicator_text;
};

class SecurityIndicatorTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<SecurityIndicatorTestParams> {
 public:
  SecurityIndicatorTest() : InProcessBrowserTest(), cert_(nullptr) {
    if (GetParam().is_enabled)
      feature_list_.InitAndEnableFeature(omnibox::kSimplifyHttpsIndicator);
    else
      feature_list_.InitAndDisableFeature(omnibox::kSimplifyHttpsIndicator);
  }

  void SetUpInProcessBrowserTestFixture() override {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ASSERT_TRUE(cert_);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  LocationBarView* GetLocationBarView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView();
  }

  void SetUpInterceptor(net::CertStatus cert_status) {
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(&SecurityIndicatorTest::InterceptURLLoad,
                            base::Unretained(this), cert_status));
  }

  void ResetInterceptor() { url_loader_interceptor_.reset(); }

  bool InterceptURLLoad(net::CertStatus cert_status,
                        content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.host() != kMockSecureHostname)
      return false;
    net::SSLInfo ssl_info;
    ssl_info.cert = cert_;
    ssl_info.cert_status = cert_status;
    ssl_info.ct_policy_compliance =
        net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
    network::ResourceResponseHead resource_response;
    resource_response.mime_type = "text/html";
    resource_response.ssl_info = ssl_info;
    params->client->OnReceiveResponse(resource_response);
    // Send an empty response's body. This pipe is not filled with data.
    mojo::DataPipe pipe;
    params->client->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));
    network::URLLoaderCompletionStatus completion_status;
    completion_status.ssl_info = ssl_info;
    params->client->OnComplete(completion_status);
    return true;
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<net::X509Certificate> cert_;

  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(SecurityIndicatorTest);
};

// Check that the security indicator text is correctly set for the various
// variations of the Security UI Study (https://crbug.com/803501).
IN_PROC_BROWSER_TEST_P(SecurityIndicatorTest, CheckIndicatorText) {
  const GURL kMockSecureURL = GURL("https://example-secure.test");
  const GURL kMockNonsecureURL =
      embedded_test_server()->GetURL("example.test", "/");

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  LocationBarView* location_bar_view = GetLocationBarView();

  auto c = GetParam();
  SetUpInterceptor(c.cert_status);
  ui_test_utils::NavigateToURL(
      browser(), c.use_secure_url ? kMockSecureURL : kMockNonsecureURL);
  EXPECT_EQ(c.security_level, helper->GetSecurityLevel());
  EXPECT_EQ(c.should_show_text,
            location_bar_view->location_icon_view()->ShouldShowLabel());
  EXPECT_EQ(c.indicator_text,
            location_bar_view->location_icon_view()->GetText());
  ResetInterceptor();
}

const base::string16 kEvString = base::ASCIIToUTF16("Test CA [US]");
const base::string16 kEmptyString = base::string16();
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SecurityIndicatorTest,
    ::testing::Values(
        // Disabled (show EV UI in omnibox)
        SecurityIndicatorTestParams{false, true, net::CERT_STATUS_IS_EV,
                                    security_state::EV_SECURE, true, kEvString},
        SecurityIndicatorTestParams{false, true, 0, security_state::SECURE,
                                    false, kEmptyString},
        SecurityIndicatorTestParams{false, false, 0, security_state::NONE,
                                    false, kEmptyString},
        // Default (lock-only in omnibox)
        SecurityIndicatorTestParams{true, true, net::CERT_STATUS_IS_EV,
                                    security_state::EV_SECURE, false,
                                    kEmptyString},
        SecurityIndicatorTestParams{true, true, 0, security_state::SECURE,
                                    false, kEmptyString},
        SecurityIndicatorTestParams{true, false, 0, security_state::NONE, false,
                                    kEmptyString}));
