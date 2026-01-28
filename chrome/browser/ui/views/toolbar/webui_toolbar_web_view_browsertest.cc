// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/command_line.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

class WebUIToolbarWebViewPixelBrowserTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewPixelBrowserTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Force the color mode to light to avoid flakiness.
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest, Basic) {
  ui::TrackedElement* element = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    element =
        BrowserElements::From(browser())->GetElement(kReloadButtonElementId);
    return element != nullptr;
  }));
  ASSERT_TRUE(element);
  views::TrackedElementViews* webui_toolbar_view_element =
      element->AsA<views::TrackedElementViews>();

  ASSERT_TRUE(webui_toolbar_view_element);
  WebUIToolbarWebView* webui_toolbar_view =
      views::AsViewClass<WebUIToolbarWebView>(
          webui_toolbar_view_element->view());
  ASSERT_TRUE(webui_toolbar_view);
  ASSERT_EQ(webui_toolbar_view->children().size(), 1u);
  views::WebView* web_view = views::AsViewClass<views::WebView>(
      webui_toolbar_view->children()[0].get());
  ASSERT_TRUE(web_view);

  // Wait for the WebView to finish composition.
  content::WaitForCopyableViewInWebContents(web_view->GetWebContents());
  // Assert that WebContents is not loading, as it affects the state of the
  // reload button.
  ASSERT_FALSE(web_view->GetWebContents()->IsLoading());
  // The WebView should be using the light color mode.
  ASSERT_EQ(web_view->GetWidget()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kLight);

  // Pixel test
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVerifyPixels)) {
    views::ViewSkiaGoldPixelDiff pixel_diff(
        "WebUIToolbarWebViewPixelBrowserTest");
    EXPECT_TRUE(pixel_diff.CompareViewScreenshot("Basic", webui_toolbar_view));
  }
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest, Accessibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ui::TrackedElement* element = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    element =
        BrowserElements::From(browser())->GetElement(kReloadButtonElementId);
    return element != nullptr;
  }));
  ASSERT_TRUE(element);
  views::TrackedElementViews* webui_toolbar_view_element =
      element->AsA<views::TrackedElementViews>();
  ASSERT_TRUE(webui_toolbar_view_element);
  WebUIToolbarWebView* webui_toolbar_view =
      views::AsViewClass<WebUIToolbarWebView>(
          webui_toolbar_view_element->view());
  ASSERT_TRUE(webui_toolbar_view);
  ASSERT_EQ(webui_toolbar_view->children().size(), 1u);
  views::WebView* web_view = views::AsViewClass<views::WebView>(
      webui_toolbar_view->children()[0].get());
  ASSERT_TRUE(web_view);

  content::WaitForCopyableViewInWebContents(web_view->GetWebContents());

  // Find accessibility node for reload button.
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Reload");
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.name = "Reload";
  ui::AXPlatformNodeDelegate* reload_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(reload_node);

  // Verify appropriate accessibility properties for reload button.
  const ui::AXNodeData& reload = reload_node->GetData();
  EXPECT_EQ(ax::mojom::Role::kButton, reload.role);
  EXPECT_EQ(true, reload.IsClickable());
  EXPECT_EQ("Reload",
            reload.GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Reload this page", reload.GetStringAttribute(
                                    ax::mojom::StringAttribute::kDescription));
  EXPECT_EQ(0, reload.GetIntAttribute(ax::mojom::IntAttribute::kHasPopup));

  // Verify enabling menu is reflected in HasPopup attribute.
  webui_toolbar_view->GetReloadControl()->SetMenuEnabled(true);
  content::WaitForAccessibilityTreeToChange(web_view->GetWebContents());
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Reload");
  reload_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(reload_node);
  EXPECT_EQ(2, reload_node->GetData().GetIntAttribute(
                   ax::mojom::IntAttribute::kHasPopup));
}
class WebUIToolbarWebViewStabilityTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewStabilityTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonMaxCrashRecoveryTimes", "1"},
           {"WebUIReloadButtonCrashRecoverResetInterval", "10s"},
           {"WebUIReloadButtonRestartUnresponsive", "true"}}},
         {features::kSkipIPCChannelPausingForNonGuests, {}},
         {features::kWebUIInProcessResourceLoadingV2, {}},
         {features::kInitialWebUISyncNavStartToCommit, {}}},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Force the color mode to light to avoid flakiness.
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

  WebUIToolbarWebView* GetWebUIToolbarWebView() {
    ui::TrackedElement* element = nullptr;
    if (!base::test::RunUntil([&]() {
          element = BrowserElements::From(browser())->GetElement(
              kReloadButtonElementId);
          return element != nullptr;
        })) {
      return nullptr;
    }
    views::TrackedElementViews* views_element =
        element->AsA<views::TrackedElementViews>();
    return views::AsViewClass<WebUIToolbarWebView>(views_element->view());
  }

  content::WebContents* GetWebContents(WebUIToolbarWebView* view) {
    return view->GetWebViewForTesting()
               ? view->GetWebViewForTesting()->GetWebContents()
               : nullptr;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the crash is recovered by reloading the page for the first time,
// but it will remain crashed for the second time, as
// `WebUIReloadButtonMaxCrashRecoveryTimes` was set to 1.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest, CrashRecovery) {
  WebUIToolbarWebView* toolbar_view = GetWebUIToolbarWebView();
  ASSERT_TRUE(toolbar_view);

  auto* web_contents = GetWebContents(toolbar_view);
  ASSERT_TRUE(web_contents);

  // Wait for the first crash and the recovery navigation.
  {
    content::TestNavigationObserver navigation_observer(web_contents);
    content::NavigationHandleObserver navigation_handle_observer(
        web_contents, GURL(chrome::kChromeUIWebUIToolbarURL));
    content::RenderProcessHostWatcher crash_observer(
        web_contents,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    web_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(
        /*exit_code=*/1);
    crash_observer.Wait();
    ASSERT_TRUE(web_contents->IsCrashed());
    navigation_observer.Wait();
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
    ASSERT_EQ(navigation_observer.last_navigation_url(),
              GURL(chrome::kChromeUIWebUIToolbarURL));
    ASSERT_TRUE(navigation_handle_observer.has_committed());
    ASSERT_FALSE(navigation_handle_observer.is_renderer_initiated());
    ASSERT_EQ(navigation_handle_observer.reload_type(),
              content::ReloadType::NORMAL);
  }

  // The `WebContents` should be reused and not crashed.
  ASSERT_EQ(GetWebContents(toolbar_view), web_contents);
  ASSERT_FALSE(web_contents->IsCrashed());
  ASSERT_EQ(web_contents->GetLastCommittedURL(),
            GURL(chrome::kChromeUIWebUIToolbarURL));

  // Wait for the second crash, there will be no recover.
  {
    content::RenderProcessHostWatcher crash_observer(
        web_contents,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    web_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.Wait();
  }

  // Verify no recovery: The WebContents should remain the same and be crashed.
  // We post a task and wait for it to run to ensure any potential recovery
  // task (which would have been posted before this) has had a chance to run.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_EQ(GetWebContents(toolbar_view), web_contents);
  ASSERT_TRUE(web_contents->IsCrashed());
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest,
                       RestartOnUnresponsive) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView();
  ASSERT_TRUE(webui_toolbar_view);
  content::WebContents* web_contents = GetWebContents(webui_toolbar_view);
  ASSERT_TRUE(web_contents);

  // Wait for the WebView to finish composition and load.
  content::WaitForCopyableViewInWebContents(web_contents);
  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  content::RenderWidgetHost* rwh = view->GetRenderWidgetHost();
  content::RenderProcessHost* rph = rwh->GetProcess();

  // Watch for process exit.
  content::RenderProcessHostWatcher crash_observer(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Watch for reload.
  content::TestNavigationObserver nav_observer(web_contents);

  // Trigger unresponsiveness.
  web_contents->GetDelegate()->RendererUnresponsive(web_contents, rwh,
                                                    base::DoNothing());

  // Wait for crash.
  crash_observer.Wait();

  // Wait for reload.
  nav_observer.Wait();

  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_FALSE(web_contents->IsCrashed());
}
