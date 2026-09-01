// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/permissions/permission_request_manager.h"
#include "components/security_state/core/security_state.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/actions/actions.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_utils.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {

class TestLocationBarObserver : public LocationBar::Observer {
 public:
  explicit TestLocationBarObserver(base::OnceClosure on_bounds_changed)
      : on_bounds_changed_(std::move(on_bounds_changed)) {}

  void OnLocationBarBoundsChanged() override {
    ASSERT_FALSE(on_bounds_changed_.is_null());
    std::move(on_bounds_changed_).Run();
  }

 private:
  base::OnceClosure on_bounds_changed_;
};

}  // namespace

class LocationBarViewBrowserTest : public InProcessBrowserTest {
 protected:
  LocationBarViewBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kWebUILocationBar);
  }

  LocationBarViewBrowserTest(const LocationBarViewBrowserTest&) = delete;
  LocationBarViewBrowserTest& operator=(const LocationBarViewBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    zoom_bubble_coordinator_ = ZoomBubbleCoordinator::From(browser());
  }

  void TearDownOnMainThread() override { zoom_bubble_coordinator_ = nullptr; }

  LocationBarView* GetLocationBarView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetLocationBarView();
  }

  page_actions::PageActionTestAccessor GetZoomAccessor() {
    return page_actions::PageActionTestAccessor(browser(),
                                                kActionShowZoomBubble);
  }

  ContentSettingImageView& GetContentSettingImageView(
      ContentSettingImageModel::ImageType image_type) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    CHECK(location_bar_view);
    return **std::ranges::find(
        location_bar_view->GetContentSettingViewsForTest(), image_type,
        &ContentSettingImageView::GetType);
  }

  bool IsContentSettingImageVisible(
      ContentSettingImageModel::ImageType image_type) {
    auto* location_bar =
        BrowserWindow::FromBrowser(browser())->GetLocationBar();
    CHECK(location_bar);
    auto* testing = location_bar->GetLocationBarForTesting();
    CHECK(testing);
    return testing->IsContentSettingImageVisible(
        ContentSettingImageModel::GetContentSettingImageModelIndexForTesting(
            image_type));
  }

  raw_ptr<ZoomBubbleCoordinator> zoom_bubble_coordinator_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure the location bar decoration is added when zooming, and is removed when
// the bubble is closed, but only if zoom was reset.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, LocationBarDecoration) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);

  EXPECT_FALSE(GetZoomAccessor().GetVisible());
  EXPECT_FALSE(zoom_bubble_coordinator_->bubble());

  // Altering zoom should display a bubble. Note ZoomBubbleView closes
  // asynchronously, so precede checks with a run loop flush.
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetZoomAccessor().GetVisible());
  EXPECT_TRUE(zoom_bubble_coordinator_->bubble());

  // Close the bubble at other than 100% zoom. Icon should remain visible.
  zoom_bubble_coordinator_->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetZoomAccessor().GetVisible());
  EXPECT_FALSE(zoom_bubble_coordinator_->bubble());

  // Show the bubble again.
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(2.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetZoomAccessor().GetVisible());
  EXPECT_TRUE(zoom_bubble_coordinator_->bubble());

  // Remains visible at 100% until the bubble is closed.
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetZoomAccessor().GetVisible());
  EXPECT_TRUE(zoom_bubble_coordinator_->bubble());

  // Closing at 100% hides the icon.
  zoom_bubble_coordinator_->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetZoomAccessor().GetVisible());
  EXPECT_FALSE(zoom_bubble_coordinator_->bubble());
}

// Ensure that middle-clicking the location icon performs a "paste and go".
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, MiddleClickPasteAndGo) {
  if (!ui::Clipboard::IsMiddleClickPasteEnabled() ||
      !ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL paste_url = embedded_test_server()->GetURL("/title1.html");

  LocationBarView* location_bar_view = GetLocationBarView();
  LocationIconView* location_icon_view =
      location_bar_view->location_icon_view();

  // Set some text in the selection clipboard.
  const std::u16string kPasteText = base::UTF8ToUTF16(paste_url.spec());
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kSelection);
    writer.WriteText(kPasteText);
  }

  // Set up an observer to wait for the navigation.
  content::TestNavigationObserver observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  // Simulate a middle-click on the location icon.
  ui::MouseEvent middle_click_event(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), base::TimeTicks::Now(),
                                    ui::EF_MIDDLE_MOUSE_BUTTON,
                                    ui::EF_MIDDLE_MOUSE_BUTTON);
  location_icon_view->OnMousePressed(middle_click_event);

  // Wait for the navigation to finish.
  observer.Wait();

  EXPECT_EQ(paste_url, browser()
                           ->GetTabStripModel()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
}

// Ensure that location bar bubbles close when the webcontents hides.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, BubblesCloseOnHide) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);

  EXPECT_FALSE(GetZoomAccessor().GetVisible());

  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetZoomAccessor().GetVisible());
  EXPECT_TRUE(zoom_bubble_coordinator_->bubble());

  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  chrome::SelectNextTab(browser());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(zoom_bubble_coordinator_->bubble());
}

// Check that the script blocked icon shows up when user disables javascript.
// Regression test for http://crbug.com/41093462
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

  // Check that the script blocked icon on the omnibox is hidden.
  EXPECT_FALSE(IsContentSettingImageVisible(
      ContentSettingImageModel::ImageType::kJavaScript));

  // Disable javascript.
  HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);
  // Reload the page
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  // Waits until the script blocked icon is visible, or aborts the tests
  // otherwise.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return IsContentSettingImageVisible(
        ContentSettingImageModel::ImageType::kJavaScript);
  })) << "Timeout waiting for the script blocked icon to become visible.";
}

IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, BoundsObserver) {
  // Make sure that bounds change observer gets notified.
  base::RunLoop run_loop;
  TestLocationBarObserver bounds_observer(run_loop.QuitClosure());
  base::ScopedObservation<LocationBar, LocationBar::Observer> obs(
      &bounds_observer);
  obs.Observe(GetLocationBarView());
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->SetSize(
      gfx::Size(browser_view->width() - 100, browser_view->height()));
  run_loop.Run();
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
    if (child != omnibox_view_views) {
      child->SetVisible(false);
    }
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
    if (child != omnibox_view_views) {
      child->SetVisible(false);
    }
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

class SecurityIndicatorTest : public LocationBarViewBrowserTest {
 public:
  SecurityIndicatorTest() = default;

  SecurityIndicatorTest(const SecurityIndicatorTest&) = delete;
  SecurityIndicatorTest& operator=(const SecurityIndicatorTest&) = delete;

  void SetUpOnMainThread() override {
    LocationBarViewBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
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
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  LocationBarView* location_bar_view = GetLocationBarView();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kMockSecureURL));
  EXPECT_EQ(security_state::SECURE,
            chrome_security_state::GetSecurityLevel(tab));
  EXPECT_FALSE(location_bar_view->location_icon_view()->ShouldShowLabel());
  EXPECT_TRUE(location_bar_view->location_icon_view()->GetText().empty());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kMockNonsecureURL));
  EXPECT_EQ(security_state::WARNING,
            chrome_security_state::GetSecurityLevel(tab));
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
    LocationBarViewBrowserTest::SetUpOnMainThread();
    // Replace any hostname to 127.0.0.1. (e.g. b.com -> 127.0.0.1)
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  content::WebContents* web_contents() const {
    return browser()->GetTabStripModel()->GetActiveWebContents();
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

  // Geolocation icon should be off in the beginning.
  EXPECT_FALSE(IsContentSettingImageVisible(
      ContentSettingImageModel::ImageType::kGeolocation));

  // Query current position, and wait for the query to complete.
  content::RenderFrameHost* rfh_a = web_contents()->GetPrimaryMainFrame();
  EXPECT_EQ("received", EvalJs(rfh_a, R"(
      new Promise(resolve => {
        navigator.geolocation.getCurrentPosition(() => resolve('received'));
      });
  )"));

  // Geolocation icon should be on since geolocation API is used.
  EXPECT_TRUE(IsContentSettingImageVisible(
      ContentSettingImageModel::ImageType::kGeolocation));

  content::RenderFrameDeletedObserver deleted(rfh_a);

  // 2) Navigate away to B.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url_b));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  content::RenderFrameHost* rfh_b = web_contents()->GetPrimaryMainFrame();

  // Geolocation icon should be off after navigation.
  EXPECT_FALSE(IsContentSettingImageVisible(
      ContentSettingImageModel::ImageType::kGeolocation));

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
  EXPECT_TRUE(IsContentSettingImageVisible(
      ContentSettingImageModel::ImageType::kGeolocation));

  // 4) Navigate forward to B. |RenderFrameHost| have to be restored from
  // BackForwardCache and be the primary main frame.
  web_contents()->GetController().GoForward();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  EXPECT_TRUE(rfh_b->IsInPrimaryMainFrame());

  // Geolocation icon should be off.
  EXPECT_FALSE(IsContentSettingImageVisible(
      ContentSettingImageModel::ImageType::kGeolocation));
}

class LocationBarViewPageActionHideWhileEditingTests
    : public LocationBarViewBrowserTest {
 public:
  LocationBarViewPageActionHideWhileEditingTests() = default;

  void SetUpOnMainThread() override {
    LocationBarViewBrowserTest::SetUpOnMainThread();

    // 1. Ensure the Zoom action is globally visible/enabled.
    auto* zoom_action =
        actions::ActionManager::Get().FindAction(kActionShowZoomBubble);
    ASSERT_TRUE(zoom_action);
    zoom_action->SetVisible(true);
    zoom_action->SetEnabled(true);

    // 2. For the active tab, actually show it in the new PageActionController.
    auto* tab_features = browser()->GetActiveTabInterface()->GetTabFeatures();
    ASSERT_TRUE(tab_features);
    page_actions::PageActionController* controller =
        tab_features->page_action_controller();
    ASSERT_TRUE(controller);
    controller->Show(kActionShowZoomBubble);

    // 3. Make the Zoom icon visible by actually adjusting page zoom from 100%.
    auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();
    auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
    ASSERT_TRUE(zoom_controller);
    zoom_controller->SetZoomLevel(
        blink::ZoomFactorToZoomLevel(/*zoom_factor=*/1.5));
  }

 protected:
  page_actions::PageActionView* GetZoomPageActionView() {
    return GetLocationBarView()->page_action_container()->GetPageActionView(
        kActionShowZoomBubble);
  }

  OmniboxView* GetOmniboxView() {
    return GetLocationBarView()->GetOmniboxView();
  }

  void EnsureLayout() { views::test::RunScheduledLayout(GetLocationBarView()); }
};

IN_PROC_BROWSER_TEST_F(LocationBarViewPageActionHideWhileEditingTests,
                       ZoomHiddenWhenOmniboxIsEdited) {
  page_actions::PageActionView* zoom_view = GetZoomPageActionView();
  ASSERT_TRUE(zoom_view);
  EXPECT_TRUE(zoom_view->GetVisible());

  // Now simulate “editing” the Omnibox:
  OmniboxView* omnibox_view = GetOmniboxView();
  omnibox_view->SetFocus(/*is_user_initiated=*/true);
  omnibox_view->SetUserText(u"Typing in the Omnibox...");
  EnsureLayout();

  // The Zoom page action should now be hidden.
  EXPECT_FALSE(zoom_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(LocationBarViewPageActionHideWhileEditingTests,
                       ZoomReAppearsAfterEditCleared) {
  page_actions::PageActionView* zoom_view = GetZoomPageActionView();
  ASSERT_TRUE(zoom_view);

  // 1) Confirm visible to start.
  EXPECT_TRUE(zoom_view->GetVisible());

  // 2) Start editing => hidden.
  OmniboxView* omnibox_view = GetOmniboxView();
  omnibox_view->SetFocus(/*is_user_initiated=*/true);
  omnibox_view->SetUserText(u"typing...");
  EnsureLayout();
  EXPECT_FALSE(zoom_view->GetVisible());

  // 3) Clear text.
  omnibox_view->SetUserText(std::u16string());
  EnsureLayout();

  // Force the Omnibox to revert (like pressing ESC).
  omnibox_view->RevertAll();

  EnsureLayout();
  EXPECT_TRUE(zoom_view->GetVisible());
}

class LocationBarViewAddContextButtonBrowserTest
    : public LocationBarViewBrowserTest {
 public:
  LocationBarViewAddContextButtonBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{omnibox::internal::kWebUIOmniboxAimPopup,
          {{omnibox::kShowToolsAndModels.name, "true"}}},
         {omnibox::internal::kWebUIOmniboxSimplification,
          {{omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
            "inline"}}},
         {omnibox::internal::kWebUIOmniboxPopup, {}},
         {omnibox::kAimEnabled, {}}},
        /*disabled_features=*/{omnibox::kAimServerEligibilityEnabled,
                               omnibox::kAimFuseboxEligibilityCheckEnabled,
                               omnibox::kAimUsePecApi});
  }

  ~LocationBarViewAddContextButtonBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/459561205): This test is flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_AddContextButtonVisibilityAndClick \
  DISABLED_AddContextButtonVisibilityAndClick
#else
#define MAYBE_AddContextButtonVisibilityAndClick \
  AddContextButtonVisibilityAndClick
#endif
IN_PROC_BROWSER_TEST_F(LocationBarViewAddContextButtonBrowserTest,
                       MAYBE_AddContextButtonVisibilityAndClick) {
  LocationBarView* location_bar_view = GetLocationBarView();
  OmniboxViewViews* omnibox_view = location_bar_view->omnibox_view();
  LocationIconView* location_icon_view =
      location_bar_view->location_icon_view();

  // The "Add Context" button doesn't show up when the Omnibox popup is
  // closed.
  EXPECT_FALSE(location_bar_view->GetOmniboxController()->IsPopupOpen());
  EXPECT_FALSE(location_bar_view->ShouldShowAddContextButton());
  const auto icon_when_closed =
      location_icon_view->GetImageModel(views::Button::STATE_NORMAL);

  // The "Add Context" button does show up when the Omnibox popup is open.
  location_bar_view->FocusLocation(/*is_user_initiated=*/true,
                                   /*clear_focus_if_failed=*/false);
  omnibox_view->SetUserText(u"test");
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return location_bar_view->GetOmniboxController()->IsPopupOpen() &&
           location_bar_view->ShouldShowAddContextButton();
  }));
  const auto icon_when_open =
      location_icon_view->GetImageModel(views::Button::STATE_NORMAL);
  EXPECT_NE(icon_when_closed->GetVectorIcon().vector_icon(),
            icon_when_open->GetVectorIcon().vector_icon());

  // Clicking on the "Add Context" button causes
  // `OmniboxContextMenu::RunMenuAt()` to get called.
  bool run_menu_called = false;
  location_bar_view->SetRunOmniboxContextMenuForTesting(
      base::BindLambdaForTesting(
          [&](OmniboxContextMenu*, gfx::Point) { run_menu_called = true; }));

  ui::MouseEvent click_event(
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  location_icon_view->OnMousePressed(click_event);

  EXPECT_TRUE(run_menu_called);
}

// TODO(crbug.com/467998506): This test is flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_PrefChangesAddContextButtonVisibility \
  DISABLED_PrefChangesAddContextButtonVisibility
#else
#define MAYBE_PrefChangesAddContextButtonVisibility \
  PrefChangesAddContextButtonVisibility
#endif
IN_PROC_BROWSER_TEST_F(LocationBarViewAddContextButtonBrowserTest,
                       MAYBE_PrefChangesAddContextButtonVisibility) {
  LocationBarView* location_bar_view = GetLocationBarView();
  OmniboxViewViews* omnibox_view = location_bar_view->omnibox_view();
  PrefService* prefs = browser()->GetProfile()->GetPrefs();

  // pref is initially true to show the button.
  prefs->SetBoolean(omnibox::kShowAiModeOmniboxButton, true);

  // Force "Add content" button to show by focusing and typing.
  location_bar_view->FocusLocation(/*is_user_initiated=*/true,
                                   /*clear_focus_if_failed=*/false);
  omnibox_view->SetUserText(u"test");
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return location_bar_view->GetOmniboxController()->IsPopupOpen();
  }));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return location_bar_view->ShouldShowAddContextButton(); }));

  // Set pref to false.
  prefs->SetBoolean(omnibox::kShowAiModeOmniboxButton, false);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !location_bar_view->ShouldShowAddContextButton(); }));
  // Set pref to true again.
  prefs->SetBoolean(omnibox::kShowAiModeOmniboxButton, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return location_bar_view->ShouldShowAddContextButton(); }));
}

IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, OmniboxActionsRegistered) {
  LocationBarView* location_bar_view = GetLocationBarView();
  ASSERT_TRUE(location_bar_view);

  auto* action_manager = &actions::ActionManager::Get();
  ASSERT_TRUE(action_manager);

  struct ExpectedAction {
    actions::ActionId action_id;
    int string_id;
  };

  std::vector<ExpectedAction> expected_actions = {
      {kActionOmniboxContextAddImage, IDS_NTP_COMPOSE_ADD_IMAGE},
      {kActionOmniboxContextAddFile, IDS_NTP_COMPOSE_ADD_FILE},
      {kActionOmniboxContextCreateImages, IDS_NTP_COMPOSE_CREATE_IMAGES},
      {kActionOmniboxContextDeepResearch, IDS_NTP_COMPOSE_DEEP_SEARCH},
      {kActionOmniboxContextCanvas, IDS_NTP_COMPOSE_CANVAS},
      {kActionOmniboxContextSetModelAuto, IDS_NTP_COMPOSE_AUTO_MODEL},
      {kActionOmniboxContextSetModelThinking, IDS_NTP_COMPOSE_THINKING_3_PRO},
  };

  for (const auto& expected : expected_actions) {
    auto* action = action_manager->FindAction(expected.action_id);
    ASSERT_TRUE(action) << "Action not found: " << expected.action_id;
    EXPECT_EQ(action->GetText(), l10n_util::GetStringUTF16(expected.string_id));
    EXPECT_FALSE(action->GetImage().IsEmpty());
  }

  // kActionOmniboxContextSetModelRegular has no text (only icon).
  auto* regular_model_action =
      action_manager->FindAction(kActionOmniboxContextSetModelRegular);
  ASSERT_TRUE(regular_model_action);
  EXPECT_TRUE(regular_model_action->GetText().empty());
  EXPECT_FALSE(regular_model_action->GetImage().IsEmpty());
}

// Tests that unsafe schemes are not allowed to be opened from middle clicks.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest,
                       MiddleClickPasteAndGoBlocksUnsafeSchemes) {
  if (!ui::Clipboard::IsMiddleClickPasteEnabled() ||
      !ui::Clipboard::IsSupportedClipboardBuffer(
          ui::ClipboardBuffer::kSelection)) {
    return;
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL start_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));

  LocationBarView* location_bar_view = GetLocationBarView();
  LocationIconView* location_icon_view =
      location_bar_view->location_icon_view();

  // Try file:// URL. Note: ui::Clipboard::ReadText is asynchronous on Linux, so
  // we must pump the runloop to allow the callback to run and be rejected.
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kSelection);
    writer.WriteText(u"file:///etc/passwd");
  }

  ui::MouseEvent middle_click_event(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), base::TimeTicks::Now(),
                                    ui::EF_MIDDLE_MOUSE_BUTTON,
                                    ui::EF_MIDDLE_MOUSE_BUTTON);
  location_icon_view->OnMousePressed(middle_click_event);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(start_url, browser()
                           ->GetTabStripModel()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());

  // Try chrome:// URL.
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kSelection);
    writer.WriteText(u"chrome://version");
  }

  location_icon_view->OnMousePressed(middle_click_event);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(start_url, browser()
                           ->GetTabStripModel()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
}

class LocationBarViewElevatedToolbarBrowserTest
    : public LocationBarViewBrowserTest {
 public:
  LocationBarViewElevatedToolbarBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPageActionsElevatedToolbar);
  }

  ~LocationBarViewElevatedToolbarBrowserTest() override = default;

 protected:
  LocationBarView* GetLocationBarView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBarView();
  }

  void EnsureLayout() { views::test::RunScheduledLayout(GetLocationBarView()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocationBarViewElevatedToolbarBrowserTest,
                       LayoutWithElevatedToolbarActiveAndInactive) {
  LocationBarView* location_bar = GetLocationBarView();
  ASSERT_TRUE(location_bar);

  auto* tab_features = browser()->GetActiveTabInterface()->GetTabFeatures();
  ASSERT_TRUE(tab_features);
  auto* controller = tab_features->page_action_controller();
  ASSERT_TRUE(controller);

  // Hide any default actions to test clean transitions.
  for (views::View* child : location_bar->page_action_container()->children()) {
    if (auto* page_action_view =
            views::AsViewClass<page_actions::PageActionView>(child)) {
      controller->Hide(page_action_view->GetActionId());
    }
  }
  EnsureLayout();

  // Layout when no page actions are visible.
  EXPECT_FALSE(location_bar->page_action_container()->IsCapsuleActive());
  EnsureLayout();

  // Make Zoom page action visible (capsule inactive, not a chip).
  auto* zoom_action =
      actions::ActionManager::Get().FindAction(kActionShowZoomBubble);
  ASSERT_TRUE(zoom_action);
  zoom_action->SetVisible(true);
  zoom_action->SetEnabled(true);
  controller->Show(kActionShowZoomBubble);

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  ASSERT_TRUE(zoom_controller);
  zoom_controller->SetZoomLevel(
      blink::ZoomFactorToZoomLevel(/*zoom_factor=*/1.5));

  EnsureLayout();
  EXPECT_TRUE(location_bar->page_action_container()->GetVisible());
  EXPECT_FALSE(location_bar->page_action_container()->IsCapsuleActive());
  EXPECT_FALSE(location_bar->page_action_container()->IsFirstVisibleViewChip());

  // Make a second action visible (capsule active).
  auto* cookie_action =
      actions::ActionManager::Get().FindAction(kActionShowCookieControls);
  if (cookie_action) {
    cookie_action->SetVisible(true);
    cookie_action->SetEnabled(true);
    controller->Show(kActionShowCookieControls);
    EnsureLayout();
    EXPECT_TRUE(location_bar->page_action_container()->IsCapsuleActive());
  }

  // Test suggestion chip branch when capsule is inactive.
  if (cookie_action) {
    controller->Hide(kActionShowCookieControls);
  }
  controller->ShowSuggestionChip(kActionShowZoomBubble);
  EnsureLayout();
  EXPECT_TRUE(location_bar->page_action_container()->IsFirstVisibleViewChip());
  EXPECT_FALSE(location_bar->page_action_container()->IsCapsuleActive());
}
