// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/command_line.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kNumMaxRecoveryTime = 2;
constexpr base::TimeDelta kRecoveryResetInterval = base::Seconds(10);
}  // namespace

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

  void SetUp() override {
    EnablePixelOutput();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Force the color mode to light to avoid flakiness.
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

  void SetUpWebUI(const ui::ElementIdentifier& element_id,
                  ui::TrackedElement** element_out,
                  WebUIToolbarWebView** webui_toolbar_view_out,
                  views::WebView** web_view_out) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      *element_out = BrowserElements::From(browser())->GetElement(element_id);
      return *element_out != nullptr;
    }));
    ASSERT_TRUE(*element_out);

    ui::TrackedElement* toolbar_element = nullptr;
    ASSERT_TRUE(base::test::RunUntil([&]() {
      toolbar_element = BrowserElements::From(browser())->GetElement(
          kWebUIToolbarElementIdentifier);
      return toolbar_element != nullptr;
    }));
    ASSERT_TRUE(toolbar_element);
    views::TrackedElementViews* webui_toolbar_view_element =
        toolbar_element->AsA<views::TrackedElementViews>();

    ASSERT_TRUE(webui_toolbar_view_element);
    *webui_toolbar_view_out = views::AsViewClass<WebUIToolbarWebView>(
        webui_toolbar_view_element->view());
    ASSERT_TRUE(*webui_toolbar_view_out);
    ASSERT_EQ((*webui_toolbar_view_out)->children().size(), 1u);
    *web_view_out = views::AsViewClass<views::WebView>(
        (*webui_toolbar_view_out)->children()[0].get());
    ASSERT_TRUE(*web_view_out);

    // Wait for the WebView to finish composition.
    content::WaitForCopyableViewInWebContents(
        (*web_view_out)->GetWebContents());
  }

  SkColor GetCenterPixelColor(views::WebView* web_view, const gfx::Rect& rect) {
    // Wait for the WebView to finish composition.
    content::WaitForCopyableViewInWebContents(web_view->GetWebContents());

    SkBitmap image;
    base::RunLoop run_loop;
    web_view->GetWebContents()->GetRenderWidgetHostView()->CopyFromSurface(
        rect, gfx::Size(), base::TimeDelta(),
        base::BindLambdaForTesting(
            [&](const content::CopyFromSurfaceResult& result) {
              ASSERT_TRUE(result.has_value());
              image = result->bitmap;
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, run_loop.QuitClosure());
            }));
    run_loop.Run();

    return image.getColor(image.width() / 2, image.height() / 2);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest, Basic) {
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kWebUIToolbarElementIdentifier, &element,
                                     &webui_toolbar_view, &web_view));

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
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kWebUIToolbarElementIdentifier, &element,
                                     &webui_toolbar_view, &web_view));

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

// TODO(crbug.com/479341115): Failing on mac-bots.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CheckReloadButtonColor DISABLED_CheckReloadButtonColor
#else
#define MAYBE_CheckReloadButtonColor CheckReloadButtonColor
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       MAYBE_CheckReloadButtonColor) {
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kReloadButtonElementId, &element,
                                     &webui_toolbar_view, &web_view));

  WebUIReloadControl* reload_control =
      static_cast<WebUIReloadControl*>(webui_toolbar_view->GetReloadControl());
  // Make sure reload icon is showing, which has a hole in the middle whose
  // pixel we'll check to see what the background color is.
  ASSERT_EQ(reload_control->mode_, ReloadControl::Mode::kReload);

  gfx::Rect control_rect = element->GetScreenBounds();
  gfx::Rect view_rect = webui_toolbar_view->GetBoundsInScreen();
  control_rect.Offset(-view_rect.OffsetFromOrigin());

  // Verify reload button background is transparent when not highlighted.
  EXPECT_EQ(GetCenterPixelColor(web_view, control_rect), SK_ColorTRANSPARENT);

  // Show reload button context menu.
  webui_toolbar_view->GetReloadControl()->SetMenuEnabled(true);
  webui_toolbar_view->HandleContextMenu(
      browser_controls_api::mojom::ContextMenuType::kReload,
      element->GetScreenBounds().bottom_right(),
      ui::mojom::MenuSourceType::kMouse);

  // Verify reload button is now highlighted.
  EXPECT_NE(GetCenterPixelColor(web_view, control_rect), SK_ColorTRANSPARENT);

  // Close reload button context menu.
  reload_control->menu_runner_->Cancel();

  // Verify reload button background returns to transparent.
  EXPECT_EQ(GetCenterPixelColor(web_view, control_rect), SK_ColorTRANSPARENT);
}

class WebUIToolbarWebViewStabilityTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewStabilityTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kWebUIReloadButton,
          {{"WebUIReloadButtonMaxCrashRecoveryTimes",
            base::ToString(kNumMaxRecoveryTime)},
           {"WebUIReloadButtonCrashRecoverResetInterval",
            base::NumberToString(kRecoveryResetInterval.InSeconds()) + "s"},
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
              kWebUIToolbarElementIdentifier);
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

// Verify that the crash is recovered by reloading the page until it hits the
// limit set in `WebUIReloadButtonMaxCrashRecoveryTimes`, after that it will
// remain crashed.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest, CrashRecovery) {
  WebUIToolbarWebView* toolbar_view = GetWebUIToolbarWebView();
  ASSERT_TRUE(toolbar_view);

  auto* web_contents = GetWebContents(toolbar_view);
  ASSERT_TRUE(web_contents);

  // Recover `kNumMaxRecoveryTime` times to hit the limit.
  for (int i = 0; i < kNumMaxRecoveryTime; ++i) {
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

    // The `WebContents` should be reused and not crashed.
    ASSERT_EQ(GetWebContents(toolbar_view), web_contents);
    ASSERT_FALSE(web_contents->IsCrashed());
    ASSERT_EQ(web_contents->GetLastCommittedURL(),
              GURL(chrome::kChromeUIWebUIToolbarURL));
  }

  // Wait for the last crash, there will be no recover.
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

// Verify that the crash recovery count resets if the interval between crashes
// exceeds the `WebUIReloadButtonCrashRecoverResetInterval`.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest,
                       CrashRecoveryWithResetInterval) {
  base::SimpleTestTickClock clock_;
  WebUIToolbarWebView* toolbar_view = GetWebUIToolbarWebView();
  ASSERT_TRUE(toolbar_view);
  toolbar_view->SetTickClockForTesting(&clock_);

  auto* web_contents = GetWebContents(toolbar_view);
  ASSERT_TRUE(web_contents);

  // Recover `kNumMaxRecoveryTime` times to hit the limit.
  for (int i = 0; i < kNumMaxRecoveryTime; ++i) {
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

    // The `WebContents` should be reused and not crashed.
    ASSERT_EQ(GetWebContents(toolbar_view), web_contents);
    ASSERT_FALSE(web_contents->IsCrashed());
    ASSERT_EQ(web_contents->GetLastCommittedURL(),
              GURL(chrome::kChromeUIWebUIToolbarURL));
  }

  clock_.Advance(base::Seconds(1) + kRecoveryResetInterval);

  // A next crash should now be recovered because the interval has passed and
  // the crash count should have been reset.
  {
    content::TestNavigationObserver navigation_observer(web_contents);
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
  }

  // The `WebContents` should be recovered and not crashed.
  ASSERT_EQ(GetWebContents(toolbar_view), web_contents);
  ASSERT_FALSE(web_contents->IsCrashed());
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

class WebUIReloadButtonBrowserTest : public InProcessBrowserTest {
 public:
  WebUIReloadButtonBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIReloadButtonBrowserTest, NoCrashOnCommandUpdate) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ToolbarView* toolbar = browser_view->toolbar();

  // Verify that the native reload button is not present.
  EXPECT_EQ(toolbar->reload_button(), nullptr);

  // Trigger a command update that would affect the reload button if it were
  // there. This calls EnabledStateChangedForCommand under the hood.
  bool enabled = browser()->command_controller()->IsCommandEnabled(IDC_RELOAD);
  browser()->command_controller()->UpdateCommandEnabled(IDC_RELOAD, !enabled);

  // Trigger a command update for something else in the list (e.g. Back)
  // to ensure iteration happens.
  enabled = browser()->command_controller()->IsCommandEnabled(IDC_BACK);
  browser()->command_controller()->UpdateCommandEnabled(IDC_BACK, !enabled);

  // Verify no crash.
}
