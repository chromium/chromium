// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_coordinator.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
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
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_utils.h"

namespace {
void FocusNextView(views::FocusManager* focus_manager) {
  views::View* const focused_view = focus_manager->GetFocusedView();
  views::View* const next_view =
      focus_manager->GetNextFocusableView(focused_view, nullptr, false, false);
  focus_manager->SetFocusedView(next_view);
}
}  // namespace

class LocationBarViewBrowserTest : public InProcessBrowserTest {
 protected:
  LocationBarViewBrowserTest() = default;

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

  views::View* GetZoomView() {
    auto* toolbar_button_provider =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider();
    return toolbar_button_provider->GetPageActionView(kActionZoomNormal);
  }

  ContentSettingImageView& GetContentSettingImageView(
      ContentSettingImageModel::ImageType image_type) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    return **std::ranges::find(
        location_bar_view->GetContentSettingViewsForTest(), image_type,
        &ContentSettingImageView::GetType);
  }

  raw_ptr<ZoomBubbleCoordinator> zoom_bubble_coordinator_;
};

// Ensure the location bar decoration is added when zooming, and is removed when
// the bubble is closed, but only if zoom was reset.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, LocationBarDecoration) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  auto* zoom_view = GetZoomView();

  ASSERT_TRUE(zoom_view);
  EXPECT_FALSE(zoom_view->GetVisible());
  EXPECT_FALSE(zoom_bubble_coordinator_->bubble());

  // Altering zoom should display a bubble. Note ZoomBubbleView closes
  // asynchronously, so precede checks with a run loop flush.
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(zoom_bubble_coordinator_->bubble());

  // Close the bubble at other than 100% zoom. Icon should remain visible.
  zoom_bubble_coordinator_->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_FALSE(zoom_bubble_coordinator_->bubble());

  // Show the bubble again.
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(2.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(zoom_bubble_coordinator_->bubble());

  // Remains visible at 100% until the bubble is closed.
  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.0));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(zoom_bubble_coordinator_->bubble());

  // Closing at 100% hides the icon.
  zoom_bubble_coordinator_->Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(zoom_view->GetVisible());
  EXPECT_FALSE(zoom_bubble_coordinator_->bubble());
}

// Ensure that location bar bubbles close when the webcontents hides.
IN_PROC_BROWSER_TEST_F(LocationBarViewBrowserTest, BubblesCloseOnHide) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  auto* zoom_view = GetZoomView();

  ASSERT_TRUE(zoom_view);
  EXPECT_FALSE(zoom_view->GetVisible());

  zoom_controller->SetZoomLevel(blink::ZoomFactorToZoomLevel(1.5));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(zoom_view->GetVisible());
  EXPECT_TRUE(zoom_bubble_coordinator_->bubble());

  chrome::NewTab(browser());
  chrome::SelectNextTab(browser());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(zoom_bubble_coordinator_->bubble());
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

class LocationBarViewPageActionsMigrationTest
    : public LocationBarViewBrowserTest {
 public:
  LocationBarViewPageActionsMigrationTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{::features::kPageActionsMigration,
          {{::features::kPageActionsMigrationLensOverlay.name, "true"}}},
         {lens::features::kLensOverlayOmniboxEntryPoint, {}}},
        {omnibox::kAiModeOmniboxEntryPoint});
  }
  ~LocationBarViewPageActionsMigrationTest() override = default;

  LocationBarViewPageActionsMigrationTest(
      const LocationBarViewPageActionsMigrationTest&) = delete;
  LocationBarViewPageActionsMigrationTest& operator=(
      const LocationBarViewPageActionsMigrationTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Wayland doesn't support changing window activation programmatically.
// TODO(crbug.com/376285664): Remove this test altogether once the migration
// is complete.
#if BUILDFLAG(IS_OZONE_WAYLAND)
#define MAYBE_LocationBarFocusOrder DISABLED_LocationBarFocusOrder
#else
#define MAYBE_LocationBarFocusOrder LocationBarFocusOrder
#endif

// Tests that shifting focus from the omnibox will focus the migrated page
// actions first, followed by the legacy page actions.
IN_PROC_BROWSER_TEST_F(LocationBarViewPageActionsMigrationTest,
                       MAYBE_LocationBarFocusOrder) {
  actions::ActionItem* const lens_action =
      actions::ActionManager::Get().FindAction(
          kActionSidePanelShowLensOverlayResults);
  ASSERT_NE(lens_action, nullptr);
  lens_action->SetVisible(true);
  lens_action->SetEnabled(true);
  browser()
      ->GetActiveTabInterface()
      ->GetTabFeatures()
      ->page_action_controller()
      ->Show(kActionSidePanelShowLensOverlayResults);

  views::View* const lens_overlay_page_action_view =
      GetLocationBarView()->page_action_container()->GetPageActionView(
          kActionSidePanelShowLensOverlayResults);
  views::View* const bookmark_page_action_view =
      GetLocationBarView()->page_action_icon_controller()->GetIconView(
          PageActionIconType::kBookmarkStar);
  ASSERT_TRUE(bookmark_page_action_view->GetVisible());

  views::FocusManager* const focus_manager =
      GetLocationBarView()->GetFocusManager();

  GetLocationBarView()->FocusLocation(true);
  OmniboxViewViews* const omnibox = GetLocationBarView()->omnibox_view();
  ASSERT_EQ(focus_manager->GetFocusedView(), omnibox);

  FocusNextView(focus_manager);
  EXPECT_EQ(focus_manager->GetFocusedView(), lens_overlay_page_action_view);

  FocusNextView(focus_manager);
  EXPECT_EQ(focus_manager->GetFocusedView(), bookmark_page_action_view);
}

class LocationBarViewPageActionHideWhileEditingTests
    : public InProcessBrowserTest {
 public:
  LocationBarViewPageActionHideWhileEditingTests() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kPageActionsMigration,
        {
            {features::kPageActionsMigrationZoom.name, "true"},
        });
  }

  void SetUpOnMainThread() override {
    // 1. Ensure the Zoom action is globally visible/enabled.
    auto* zoom_action =
        actions::ActionManager::Get().FindAction(kActionZoomNormal);
    ASSERT_TRUE(zoom_action);
    zoom_action->SetVisible(true);
    zoom_action->SetEnabled(true);

    // 2. For the active tab, actually show it in the new PageActionController.
    auto* tab_features = browser()->GetActiveTabInterface()->GetTabFeatures();
    ASSERT_TRUE(tab_features);
    page_actions::PageActionController* controller =
        tab_features->page_action_controller();
    ASSERT_TRUE(controller);
    controller->Show(kActionZoomNormal);

    // 3. Make the Zoom icon visible by actually adjusting page zoom from 100%.
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
    ASSERT_TRUE(zoom_controller);
    zoom_controller->SetZoomLevel(
        blink::ZoomFactorToZoomLevel(/*zoom_factor=*/1.5));
  }

 protected:
  page_actions::PageActionView* GetZoomPageActionView() {
    return GetLocationBarView()->page_action_container()->GetPageActionView(
        kActionZoomNormal);
  }

  LocationBarView* GetLocationBarView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBarView();
  }

  OmniboxView* GetOmniboxView() {
    return GetLocationBarView()->GetOmniboxView();
  }

  void EnsureLayout() { views::test::RunScheduledLayout(GetLocationBarView()); }

  base::test::ScopedFeatureList scoped_feature_list_;
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
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{omnibox::internal::kWebUIOmniboxAimPopup,
          {{omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.name,
            "inline"}}},
         {omnibox::kWebUIOmniboxPopup, {}}},
        /*disabled_features=*/{omnibox::kAimServerEligibilityEnabled});
  }
  ~LocationBarViewAddContextButtonBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/454313733): This test is flaky on Linux.
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
  EXPECT_FALSE(location_bar_view->GetOmniboxController()
                   ->edit_model()
                   ->ShouldShowAddContextButton());
  const auto icon_when_closed =
      location_icon_view->GetImageModel(views::Button::STATE_NORMAL);

  // The "Add Context" button does show up when the Omnibox popup is open.
  location_bar_view->FocusLocation(true);
  omnibox_view->SetUserText(u"test");
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return location_bar_view->GetOmniboxController()->IsPopupOpen();
  }));
  EXPECT_TRUE(location_bar_view->GetOmniboxController()
                  ->edit_model()
                  ->ShouldShowAddContextButton());
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
