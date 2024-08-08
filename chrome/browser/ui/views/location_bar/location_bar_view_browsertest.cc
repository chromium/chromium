// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
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
#include "components/permissions/permission_request_manager.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/ct_policy_status.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_utils.h"

class LocationBarViewBrowserTest : public InProcessBrowserTest {
 public:
  LocationBarViewBrowserTest() = default;

  LocationBarViewBrowserTest(const LocationBarViewBrowserTest&) = delete;
  LocationBarViewBrowserTest& operator=(const LocationBarViewBrowserTest&) =
      delete;

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

  ContentSettingImageView& GetContentSettingImageView(
      ContentSettingImageModel::ImageType image_type) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    return **base::ranges::find(
        location_bar_view->GetContentSettingViewsForTest(), image_type,
        &ContentSettingImageView::GetType);
  }
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
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Close the bubble at other than 100% zoom. Icon should remain visible.
  ZoomBubbleView::CloseCurrentBubble();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Show the bubble again.
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(2.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  // Remains visible at 100% until the bubble is closed.
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.0));
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

  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());

  chrome::NewTab(browser());
  chrome::SelectNextTab(browser());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
}

// Check that the script blocked icon shows up when user disables javascript.
// Regression test for http://crbug.com/35011
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, ScriptBlockedIcon) {
  const char kHtml[] =
      "<html>"
      "<head>"
      "<script>document.createElement('div');</script>"
      "</head>"
      "<body>"
      "</body>"
      "</html>";

  GURL url(std::string("data:text/html,") + kHtml);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Get the script blocked icon on the omnibox. It should be hidden.
  ContentSettingImageView& script_blocked_icon = GetContentSettingImageView(
      ContentSettingImageModel::ImageType::JAVASCRIPT);
  EXPECT_FALSE(script_blocked_icon.GetVisible());

  // Disable javascript.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);
  // Reload the page
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  // Waits until the geolocation icon is visible, or aborts the tests otherwise.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return script_blocked_icon.GetVisible();
  })) << "Timeout waiting for the script blocked icon to become visible.";
}

class TouchLocationBarViewBrowserTest : public LocationBarViewBrowserTest {
 public:
  TouchLocationBarViewBrowserTest() = default;

 private:
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_{true};
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

  views::test::RunScheduledLayout(GetLocationBarView());
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
  omnibox_view_views->SetText(u"谷");
  GetLocationBarView()->SetImeInlineAutocompletion(u"歌");
  EXPECT_TRUE(ime_inline_autocomplete_view->GetVisible());

  GetLocationBarView()->DeprecatedLayoutImmediately();

  // Make sure the IME inline autocomplete view starts at the end of
  // |omnibox_view_views|.
  EXPECT_EQ(omnibox_view_views->bounds().right(),
            ime_inline_autocomplete_view->x());
}

IN_PROC_BROWSER_TEST_F(TouchLocationBarViewBrowserTest, AccessibleProperties) {
  auto* view = GetLocationBarView();
  ui::AXNodeData data;

  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kGroup);
}

class SecurityIndicatorTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  SecurityIndicatorTest() = default;

  SecurityIndicatorTest(const SecurityIndicatorTest&) = delete;
  SecurityIndicatorTest& operator=(const SecurityIndicatorTest&) = delete;

  LocationBarView* GetLocationBarView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView();
  }
};

// Check that the security indicator text is not shown for HTTPS and "Not
// secure" is shown for HTTP.
IN_PROC_BROWSER_TEST_F(SecurityIndicatorTest, CheckIndicatorText) {
  net::EmbeddedTestServer secure_server(net::EmbeddedTestServer::TYPE_HTTPS);
  secure_server.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
  secure_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(secure_server.Start());
  const GURL kMockSecureURL = secure_server.GetURL("a.test", "/empty.html");

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kMockNonsecureURL =
      embedded_test_server()->GetURL("example.test", "/empty.html");

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  SecurityStateTabHelper* helper = SecurityStateTabHelper::FromWebContents(tab);
  ASSERT_TRUE(helper);
  LocationBarView* location_bar_view = GetLocationBarView();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kMockSecureURL));
  EXPECT_EQ(security_state::SECURE, helper->GetSecurityLevel());
  EXPECT_FALSE(location_bar_view->location_icon_view()->ShouldShowLabel());
  EXPECT_TRUE(location_bar_view->location_icon_view()->GetText().empty());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kMockNonsecureURL));
  EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
  EXPECT_TRUE(location_bar_view->location_icon_view()->ShouldShowLabel());
  EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(
      location_bar_view->location_icon_view()->GetText(), "not secure"));
}

class LocationBarViewGeolocationBackForwardCacheBrowserTest
    : public LocationBarViewBrowserTest {
 public:
  LocationBarViewGeolocationBackForwardCacheBrowserTest()
      : geo_override_(0.0, 0.0) {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kLoadingTasksUnfreezable, {}},
             {features::kBackForwardCacheMemoryControls, {}}},
            /*ignore_outstanding_network_request=*/false),
        {});
  }

  void SetUpOnMainThread() override {
    // Replace any hostname to 127.0.0.1. (e.g. b.com -> 127.0.0.1)
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  device::ScopedGeolocationOverrider geo_override_;
  base::test::ScopedFeatureList feature_list_;
};

// Check that the geolocation icon on the omnibox is on when
// geolocation is requested. After navigating away, the geolocation
// icon should be turned off even if the page is kept on BFCache. When
// navigating back to the page which requested geolocation, the
// geolocation icon should be turned on again.
IN_PROC_BROWSER_TEST_F(LocationBarViewGeolocationBackForwardCacheBrowserTest,
                       CheckGeolocationIconVisibility) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Give automatic geolocation permission.
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);

  // 1) Navigate to A.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_a));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Get the geolocation icon on the omnibox.
  ContentSettingImageView& geolocation_icon = GetContentSettingImageView(
      ContentSettingImageModel::ImageType::GEOLOCATION);

  // Geolocation icon should be off in the beginning.
  EXPECT_FALSE(geolocation_icon.GetVisible());

  // Query current position, and wait for the query to complete.
  content::RenderFrameHost* rfh_a = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ("received", EvalJs(rfh_a, R"(
      new Promise(resolve => {
        navigator.geolocation.getCurrentPosition(() => resolve('received'));
      });
  )"));

  // Geolocation icon should be on since geolocation API is used.
  EXPECT_TRUE(geolocation_icon.GetVisible());

  content::RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  content::RenderFrameHost* rfh_b = web_contents()->GetPrimaryMainFrame();

  // Geolocation icon should be off after navigation.
  EXPECT_FALSE(geolocation_icon.GetVisible());

  // The previous page should be bfcached.
  EXPECT_FALSE(deleted.deleted());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 3) Navigate back to A. |RenderFrameHost| have to be restored from
  // BackForwardCache and be the primary main frame.
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_TRUE(rfh_a->IsInPrimaryMainFrame());
  EXPECT_EQ(rfh_b->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Geolocation icon should be on again.
  EXPECT_TRUE(geolocation_icon.GetVisible());

  // 4) Navigate forward to B. |RenderFrameHost| have to be restored from
  // BackForwardCache and be the primary main frame.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());

  // Geolocation icon should be off.
  EXPECT_FALSE(geolocation_icon.GetVisible());
}
