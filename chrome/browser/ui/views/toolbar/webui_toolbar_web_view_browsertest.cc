// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_switches.h"
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
constexpr base::TimeDelta kRecoveryRetryInterval = base::Seconds(20);

constexpr char kSplitTabsSelector[] = "split-tabs-button-app";
constexpr char kReloadButtonSelector[] = "reload-button-app";

std::string GetButtonAppJS(const std::string& selector) {
  return base::StringPrintf(
      "document.querySelector('toolbar-app')?.shadowRoot?.querySelector('%s')",
      selector.c_str());
}

std::string GetButtonIconJS(const std::string& selector) {
  return base::StrCat({GetButtonAppJS(selector),
                       "?.shadowRoot?.querySelector('cr-icon-button')"});
}

std::string GetValueForCSSProperty(const std::string& element_js,
                                   const std::string& property) {
  return base::StringPrintf(
      "window.getComputedStyle(%s).getPropertyValue('%s')", element_js.c_str(),
      property.c_str());
}

std::string GetValueForToolbarAppCSSProperty(const std::string& property) {
  return GetValueForCSSProperty("document.querySelector('toolbar-app')",
                                property);
}

bool WaitForButtonVisible(content::WebContents* web_contents,
                          const std::string& selector) {
  static constexpr char kScript[] = R"(
    (() => {
      const btn = %s;
      return !!btn && btn.checkVisibility();
    })();
  )";

  return base::test::RunUntil([&]() {
    return content::EvalJs(
               web_contents,
               base::StringPrintf(kScript, GetButtonAppJS(selector).c_str()))
        .ExtractBool();
  });
}

WebUIToolbarWebView* GetWebUIToolbarWebView(Browser* browser) {
  return static_cast<ToolbarButtonProvider*>(
             BrowserView::GetBrowserViewForBrowser(browser)->toolbar())
      ->GetWebUIToolbarViewForTesting();
}

void PinSplitTabsButton(Browser* browser, views::WebView* web_view) {
  browser->profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton, true);
  content::WaitForCopyableViewInWebContents(web_view->GetWebContents());
}

// Dispatches an event to the Split Tabs Button.
// `event_class`: The JS event class (e.g. 'MouseEvent', 'PointerEvent').
// `type`: The event type string (e.g. 'click', 'contextmenu').
// `options`: JS object string for event options (e.g. "detail: 1, button: 2").
std::string DispatchEventScript(const std::string& event_class,
                                const std::string& type,
                                const std::string& options = "") {
  return base::StringPrintf(
      "%s?.dispatchEvent(new %s('%s', "
      "{bubbles: true, cancelable: true, view: window, %s}));",
      GetButtonIconJS(kSplitTabsSelector).c_str(), event_class.c_str(),
      type.c_str(), options.c_str());
}

class NavigationCounter : public content::WebContentsObserver {
 public:
  explicit NavigationCounter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigation_count_++;
  }

  size_t navigation_count() const { return navigation_count_; }

 private:
  size_t navigation_count_ = 0;
};

}  // namespace

class WebUIToolbarWebViewPixelBrowserTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewPixelBrowserTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUISplitTabsButton,
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
    // Wait for the WebUIToolbarWebView to be available.
    *webui_toolbar_view_out = nullptr;
    ASSERT_TRUE(base::test::RunUntil([&]() {
      BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser());
      if (!browser_view || !browser_view->toolbar()) {
        return false;
      }
      ToolbarButtonProvider* provider = browser_view->toolbar();
      *webui_toolbar_view_out = provider->GetWebUIToolbarViewForTesting();
      return *webui_toolbar_view_out != nullptr;
    }));
    ASSERT_TRUE(*webui_toolbar_view_out);

    if (element_id == kWebUIToolbarElementIdentifier) {
      // We already have the view, and the Basic test doesn't strictly need the
      // TrackedElement. ElementTracker might be flaky or slow here.
      *element_out =
          views::ElementTrackerViews::GetInstance()->GetElementForView(
              *webui_toolbar_view_out);
    } else {
      ASSERT_TRUE(base::test::RunUntil([&]() {
        *element_out = BrowserElements::From(browser())->GetElement(element_id);
        return *element_out != nullptr;
      }));
      ASSERT_TRUE(*element_out);
    }

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
  webui_toolbar_view->GetReloadControl()->SetDevToolsStatus(true);
  content::WaitForAccessibilityTreeToChange(web_view->GetWebContents());
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Reload");
  reload_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(reload_node);
  EXPECT_EQ(2, reload_node->GetData().GetIntAttribute(
                   ax::mojom::IntAttribute::kHasPopup));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       CheckReloadButtonColor) {
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
  webui_toolbar_view->GetReloadControl()->SetDevToolsStatus(true);
  webui_toolbar_view->HandleContextMenu(
      toolbar_ui_api::mojom::ContextMenuType::kReload,
      element->GetScreenBounds().bottom_right(),
      ui::mojom::MenuSourceType::kMouse);

  // Verify reload button is now highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, control_rect) != SK_ColorTRANSPARENT;
  }));

  // Close reload button context menu.
  reload_control->menu_runner_->Cancel();

  // Verify reload button background returns to transparent.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, control_rect) == SK_ColorTRANSPARENT;
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       CheckSplitTabsButtonColor) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton, true);

  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kToolbarSplitTabsToolbarButtonElementId,
                                     &element, &webui_toolbar_view, &web_view));

  WebUISplitTabsControl* split_tabs_control =
      &webui_toolbar_view->split_tabs_control_;

  gfx::Rect control_rect = element->GetScreenBounds();
  gfx::Rect view_rect = webui_toolbar_view->GetBoundsInScreen();
  // Wait for the reload button to finish laying out, which should
  // push the split tabs button over by at least one button width.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    control_rect = element->GetScreenBounds();
    return (control_rect.x() - view_rect.x()) >
           GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  }));

  control_rect.Offset(-view_rect.OffsetFromOrigin());

  // Sample a point in the background area (e.g. 5,5 from top-left).
  gfx::Rect background_probe_rect(control_rect.x() + 5, control_rect.y() + 5, 1,
                                  1);

  // Verify initial state is transparent.
  EXPECT_EQ(GetCenterPixelColor(web_view, background_probe_rect),
            SK_ColorTRANSPARENT);

  // Show context menu.
  split_tabs_control->HandleContextMenu(
      toolbar_ui_api::mojom::ContextMenuType::kSplitTabsContext,
      element->GetScreenBounds().bottom_right(),
      ui::mojom::MenuSourceType::kMouse);

  // Verify background is highlighted (NOT transparent).
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) !=
           SK_ColorTRANSPARENT;
  }));

  // Close context menu.
  split_tabs_control->menu_runner_->Cancel();

  // Verify split tabs button background returns to transparent.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) ==
           SK_ColorTRANSPARENT;
  }));
}

class WebUIToolbarWebViewStabilityTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewStabilityTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {}},
         {features::kWebUIReloadButton,
          {
              {"WebUIReloadButtonMaxCrashRecoveryTimes",
               base::ToString(kNumMaxRecoveryTime)},
              {"WebUIReloadButtonCrashRecoverResetInterval",
               base::NumberToString(kRecoveryResetInterval.InSeconds()) + "s"},
              {"WebUIReloadButtonRestartUnresponsive", "true"},
              {"WebUIReloadButtonCrashRecoverRetryInterval",
               base::NumberToString(kRecoveryRetryInterval.InSeconds()) + "s"},
          }},
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
    WebUIToolbarWebView* webui_toolbar_view = nullptr;
    if (!base::test::RunUntil([&]() {
          BrowserView* browser_view =
              BrowserView::GetBrowserViewForBrowser(browser());
          if (!browser_view) {
            return false;
          }
          ToolbarView* toolbar = browser_view->toolbar();
          if (!toolbar) {
            return false;
          }
          ToolbarButtonProvider* provider = toolbar;
          webui_toolbar_view = provider->GetWebUIToolbarViewForTesting();
          return webui_toolbar_view != nullptr;
        })) {
      return nullptr;
    }
    return webui_toolbar_view;
  }

  content::WebContents* GetWebContents(WebUIToolbarWebView* view) {
    return view->GetWebViewForTesting()
               ? view->GetWebViewForTesting()->GetWebContents()
               : nullptr;
  }

 protected:
  void KillRendererUntilReachingLimit(WebUIToolbarWebView* toolbar_view,
                                      content::WebContents* web_contents) {
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
  }

  void KillRendererUntilExceedingLimit(WebUIToolbarWebView* toolbar_view,
                                       content::WebContents* web_contents) {
    KillRendererUntilReachingLimit(toolbar_view, web_contents);

    // Wait for the last crash, there will be no recover.
    content::RenderProcessHostWatcher crash_observer(
        web_contents,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    web_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.Wait();

    // Verify that the WebContents should remain the same and be crashed.
    // We post a task and wait for it to run to ensure any potential recovery
    // task (which would have been posted before this) has had a chance to run.
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_EQ(GetWebContents(toolbar_view), web_contents);
    ASSERT_TRUE(web_contents->IsCrashed());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the crash is recovered by reloading the page until it hits the
// limit set in `WebUIReloadButtonMaxCrashRecoveryTimes`, after that it will
// remain crashed.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest,
                       CrashRecovery_CrashLimit) {
  WebUIToolbarWebView* toolbar_view = GetWebUIToolbarWebView();
  ASSERT_TRUE(toolbar_view);

  auto* web_contents = GetWebContents(toolbar_view);
  ASSERT_TRUE(web_contents);

  KillRendererUntilExceedingLimit(toolbar_view, web_contents);
}

// Verify that the crash is recovered after the retry interval even after it
// hits the limit set in `WebUIReloadButtonMaxCrashRecoveryTimes`.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest,
                       CrashRecovery_CrashRetry) {
  base::SimpleTestTickClock clock_;
  WebUIToolbarWebView* toolbar_view = GetWebUIToolbarWebView();
  ASSERT_TRUE(toolbar_view);
  toolbar_view->SetTickClockForTesting(&clock_);

  auto* web_contents = GetWebContents(toolbar_view);
  ASSERT_TRUE(web_contents);

  KillRendererUntilExceedingLimit(toolbar_view, web_contents);

  // Verify that the renderer is recovered after `kRecoveryRetryInterval` when
  // the recover limit is reached.
  clock_.Advance(base::Seconds(1) + kRecoveryRetryInterval);
  {
    content::TestNavigationObserver navigation_observer(web_contents);
    content::NavigationHandleObserver navigation_handle_observer(
        web_contents, GURL(chrome::kChromeUIWebUIToolbarURL));

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
}

// Verify that the crash recovery count resets if the interval between crashes
// exceeds the `WebUIReloadButtonCrashRecoverResetInterval`.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest,
                       CrashRecovery_ResetInterval) {
  base::SimpleTestTickClock clock_;
  WebUIToolbarWebView* toolbar_view = GetWebUIToolbarWebView();
  ASSERT_TRUE(toolbar_view);
  toolbar_view->SetTickClockForTesting(&clock_);

  auto* web_contents = GetWebContents(toolbar_view);
  ASSERT_TRUE(web_contents);

  KillRendererUntilReachingLimit(toolbar_view, web_contents);

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

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest,
                       CrashDuringBrowserClose) {
  WebUIToolbarWebView* toolbar_view = GetWebUIToolbarWebView();
  ASSERT_TRUE(toolbar_view);
  content::WebContents* web_contents = GetWebContents(toolbar_view);
  ASSERT_TRUE(web_contents);

  // Add a beforeunload handler to the active tab to pause the close process.
  ASSERT_TRUE(
      content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.addEventListener('beforeunload', "
                      "function(event) { event.returnValue = 'Foo'; });"));
  content::PrepContentsForBeforeUnloadTest(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Close the window. This should trigger the beforeunload dialog and set the
  // browser into the "attempting to close" state.
  browser()->window()->Close();

  // Verify the browser is attempting to close.
  EXPECT_TRUE(browser()->capabilities()->IsAttemptingToCloseBrowser());

  // Watch for reload.
  content::NavigationHandleObserver nav_observer(
      web_contents, GURL(chrome::kChromeUIWebUIToolbarURL));

  // Crash the WebUI renderer.
  content::RenderProcessHost* process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(1);
  crash_observer.Wait();

  // Run the loop to ensure any posted recovery tasks would have started.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Verify that the WebContents is still crashed and no reload happened.
  EXPECT_TRUE(web_contents->IsCrashed());
  EXPECT_FALSE(nav_observer.has_committed());

  // Cleanup: Accept the beforeunload dialog to allow the browser to close.
  ui_test_utils::WaitForAppModalDialog();
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::JavaScriptDialogManager* dialog_manager =
      static_cast<content::WebContentsDelegate*>(browser())
          ->GetJavaScriptDialogManager(active_web_contents);
  ui_test_utils::BrowserDestroyedObserver observer(browser());
  dialog_manager->HandleJavaScriptDialog(active_web_contents, /*accept=*/true,
                                         /*prompt_override=*/nullptr);
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest,
                       NoRedundantNavigationOnReparenting) {
  // 1. Setup: Create the view.
  auto webui_toolbar_view = std::make_unique<WebUIToolbarWebView>(
      browser(), browser()->command_controller(), /*location_bar=*/nullptr);

  content::WebContents* web_contents =
      webui_toolbar_view->GetWebViewForTesting()->GetWebContents();
  NavigationCounter nav_observer(web_contents);

  // Helper to create a test widget.
  auto create_widget = [&]() {
    auto widget = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.context = browser()->window()->GetNativeWindow();
    params.bounds = gfx::Rect(0, 0, 100, 100);
    widget->Init(std::move(params));
    return widget;
  };

  // 2. Initial Add: Triggers kUninitialized -> kPending.
  auto widget1 = create_widget();
  WebUIToolbarWebView* view_ptr =
      widget1->GetContentsView()->AddChildView(std::move(webui_toolbar_view));
  EXPECT_EQ(nav_observer.navigation_count(), 1u);
  EXPECT_TRUE(view_ptr->IsPendingForTesting());

  // 3. Simulate reparenting: Move to widget2 while in kPending state.
  // RemoveChildViewT returns a unique_ptr to safely move the view.
  auto moved_view = widget1->GetContentsView()->RemoveChildViewT(view_ptr);

  auto widget2 = create_widget();
  widget2->GetContentsView()->AddChildView(std::move(moved_view));

  // 4. Verification: The navigation count must still be 1.
  EXPECT_EQ(nav_observer.navigation_count(), 1u);

  widget2->CloseNow();
  widget1->CloseNow();
}

class WebUIToolbarWebViewBrowserTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUISplitTabsButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewBrowserTest,
                       ToggleSplitTabsButtonVisibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(webui_toolbar_view);
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);

  // Initially, the button should NOT be visible (default is unpinned).
  std::string button_name =
      l10n_util::GetStringUTF8(IDS_ACCNAME_SPLIT_TABS_TOOLBAR_BUTTON_PINNED);

  PinSplitTabsButton(browser(), web_view);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));

  // Wait for it to appear in accessibility tree.
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), button_name);

  // Verify accessibility properties.
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.name = button_name;
  find_criteria.role = ax::mojom::Role::kButton;
  ui::AXPlatformNodeDelegate* split_tabs_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(split_tabs_node);

  // Disable the button via pref.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton,
                                               false);
  // Wait for the tree to change.
  content::WaitForAccessibilityTreeToChange(web_view->GetWebContents());

  // Verify it is gone.
  split_tabs_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  EXPECT_FALSE(split_tabs_node);
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewBrowserTest,
                       VerifyDynamicTouchModeUpdate) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  PinSplitTabsButton(browser(), web_view);
  ASSERT_TRUE(WaitForButtonVisible(web_contents, kReloadButtonSelector));
  ASSERT_TRUE(WaitForButtonVisible(web_contents, kSplitTabsSelector));

  // Initial state: Standard (Touch disabled).
  EXPECT_EQ("34px",
            content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                              "--toolbar-button-height"))
                .ExtractString());
  EXPECT_EQ("20px",
            content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                              "--toolbar-button-icon-size"))
                .ExtractString());
  EXPECT_EQ("2px",
            content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                              "--toolbar-icon-default-margin"))
                .ExtractString());
  EXPECT_EQ("1px", content::EvalJs(web_contents,
                                   GetValueForCSSProperty(
                                       GetButtonAppJS(kSplitTabsSelector),
                                       "--split-tabs-indicator-spacing"))
                       .ExtractString());
  std::string get_indicator_bottom_js = base::StringPrintf(
      "window.getComputedStyle("
      "%s.shadowRoot.querySelector('.status-indicator')).bottom",
      GetButtonAppJS(kSplitTabsSelector).c_str());
  EXPECT_EQ(
      "4px",
      content::EvalJs(web_contents, get_indicator_bottom_js).ExtractString());

  // Enable Touch UI.
  {
    ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper(true);

    // Wait for the WebUI to update CSS variables.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                               "--toolbar-button-height"))
                 .ExtractString() == "48px";
    }));

    EXPECT_EQ("24px",
              content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                                "--toolbar-button-icon-size"))
                  .ExtractString());
    EXPECT_EQ("0px", content::EvalJs(web_contents,
                                     GetValueForToolbarAppCSSProperty(
                                         "--toolbar-icon-default-margin"))
                         .ExtractString());
    EXPECT_EQ(
        "9px",
        content::EvalJs(web_contents, get_indicator_bottom_js).ExtractString());
  }

  // Verify revert to Standard.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                             "--toolbar-button-height"))
               .ExtractString() == "34px";
  }));
  EXPECT_EQ("2px",
            content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                              "--toolbar-icon-default-margin"))
                .ExtractString());
  EXPECT_EQ(
      "4px",
      content::EvalJs(web_contents, get_indicator_bottom_js).ExtractString());
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

class WebUIToolbarWebViewSplitTabsBrowserTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewSplitTabsBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUISplitTabsButton,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2,
         features::kInitialWebUISyncNavStartToCommit},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       CheckSplitTabsButtonSourceType) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  WebUISplitTabsControl* split_tabs_control =
      &webui_toolbar_view->split_tabs_control_;

  // Create split [A, B].
  chrome::NewSplitTab(browser(),
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_model->GetActiveTab()->IsSplit(); }));

  // Wait for the button to know it is in split state.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               web_view->GetWebContents(),
               base::StrCat({GetButtonAppJS(kSplitTabsSelector),
                             "?.state?.isCurrentTabSplit === true"}))
        .ExtractBool();
  }));

  const struct {
    const char* name;
    std::string script;
    ui::mojom::MenuSourceType expected_source;
  } kTestCases[] = {
      {"Keyboard Click",
       DispatchEventScript("MouseEvent", "click", "detail: 0"),
       ui::mojom::MenuSourceType::kKeyboard},
      {"Mouse Click", DispatchEventScript("MouseEvent", "click", "detail: 1"),
       ui::mojom::MenuSourceType::kMouse},
      {"Touch Click",
       DispatchEventScript("PointerEvent", "click",
                           "pointerType: 'touch', detail: 1"),
       ui::mojom::MenuSourceType::kTouch},
      {"Pen Click",
       DispatchEventScript("PointerEvent", "click",
                           "pointerType: 'pen', detail: 1"),
       ui::mojom::MenuSourceType::kTouch},
      {"Keyboard Context Menu",
       DispatchEventScript("MouseEvent", "contextmenu", "detail: 0"),
       ui::mojom::MenuSourceType::kKeyboard},
      {"Mouse Context Menu",
       DispatchEventScript("MouseEvent", "contextmenu", "button: 2"),
       ui::mojom::MenuSourceType::kMouse},
      {"Touch Context Menu",
       DispatchEventScript("PointerEvent", "contextmenu",
                           "pointerType: 'touch'"),
       ui::mojom::MenuSourceType::kTouch},
      {"Pen Context Menu",
       DispatchEventScript("PointerEvent", "contextmenu", "pointerType: 'pen'"),
       ui::mojom::MenuSourceType::kTouch},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);
    EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(), test_case.script));
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return split_tabs_control->last_source_type_for_testing_ ==
             test_case.expected_source;
    }));
    split_tabs_control->menu_runner_->Cancel();
  }
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       ClickSplitTabsButton) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinSplitTabsButton(browser(), web_view);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));

  // Ensure NOT in split view initially.
  auto* tab_strip_model = browser()->tab_strip_model();
  EXPECT_FALSE(tab_strip_model->GetActiveTab()->IsSplit());

  EXPECT_TRUE(
      content::ExecJs(web_view->GetWebContents(),
                      DispatchEventScript("MouseEvent", "click", "detail: 1")));

  // Verify entered split view. This might take a moment, so need to wait.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_model->GetActiveTab()->IsSplit(); }));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       SplitTabsButtonAriaHasPopup) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  PinSplitTabsButton(browser(), web_view);
  ASSERT_TRUE(WaitForButtonVisible(web_contents, kSplitTabsSelector));

  // Initially NOT split. aria-haspopup should be 'false'.
  const std::string kGetAriaHasPopup =
      base::StrCat({GetButtonIconJS(kSplitTabsSelector),
                    "?.getAttribute('aria-haspopup') || 'false'"});
  EXPECT_EQ("false", content::EvalJs(web_contents, kGetAriaHasPopup));

  EXPECT_TRUE(content::ExecJs(
      web_contents, DispatchEventScript("MouseEvent", "click", "detail: 1")));

  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_model->GetActiveTab()->IsSplit(); }));

  // Now split. aria-haspopup should be 'menu'.
  // The state update is async from the browser back to the WebUI.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents, kGetAriaHasPopup).ExtractString() ==
           "menu";
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       RightClickSplitTabsButton) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinSplitTabsButton(browser(), web_view);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));
  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchEventScript("MouseEvent", "contextmenu", "button: 2")));

  // Verify no crash.
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       ClickSplitTabsButtonWhileSplit) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinSplitTabsButton(browser(), web_view);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));

  // Create a split tab group manually to simulate being in split mode.
  chrome::NewSplitTab(browser(),
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_model->GetActiveTab()->IsSplit(); }));

  // Click the button while in split mode.
  EXPECT_TRUE(
      content::ExecJs(web_view->GetWebContents(),
                      DispatchEventScript("MouseEvent", "click", "detail: 1")));

  // Verify no crash.
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       VerifySplitTabLocations) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinSplitTabsButton(browser(), web_view);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));

  // Create split [A, B]. A is active.
  chrome::NewSplitTab(browser(),
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_model->GetActiveTab()->IsSplit(); }));

  // Verify icon is 'split-scene-right' (kEnd) because new tab is active and on
  // the right.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_view->GetWebContents(),
                           base::StrCat({GetButtonIconJS(kSplitTabsSelector),
                                         "?.getAttribute('iron-icon') || ''"}))
               .ExtractString() == "split-tabs-button:split-scene-right";
  }));

  // Activate the other tab (Left/Start).
  int other_index = tab_strip_model->active_index() == 0 ? 1 : 0;
  tab_strip_model->ActivateTabAt(other_index);

  // Verify icon is 'split-scene-left' (kStart).
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_view->GetWebContents(),
                           base::StrCat({GetButtonIconJS(kSplitTabsSelector),
                                         "?.getAttribute('iron-icon') || ''"}))
               .ExtractString() == "split-tabs-button:split-scene-left";
  }));
}

class WebUIToolbarWebViewTouchBrowserTest
    : public WebUIToolbarWebViewSplitTabsBrowserTest {
 public:
  WebUIToolbarWebViewTouchBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIToolbarWebViewSplitTabsBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kTopChromeTouchUi,
                                    switches::kTopChromeTouchUiEnabled);
  }
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewTouchBrowserTest, VerifyLayout) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  PinSplitTabsButton(browser(), web_view);
  ASSERT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));

  // Verify CSS variables set by app.ts based on loadTimeData.
  // Toolbar button height should be 48px in touch mode (vs 34px).
  EXPECT_EQ("48px",
            content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                              "--toolbar-button-height"))
                .ExtractString());

  // Toolbar icon size should be 24px in touch mode (vs 20px).
  EXPECT_EQ("24px",
            content::EvalJs(web_contents, GetValueForToolbarAppCSSProperty(
                                              "--toolbar-button-icon-size"))
                .ExtractString());

  // Spacing should be 1px.
  EXPECT_EQ("1px", content::EvalJs(web_contents,
                                   GetValueForCSSProperty(
                                       GetButtonAppJS(kSplitTabsSelector),
                                       "--split-tabs-indicator-spacing"))
                       .ExtractString());

  // Verify computed style for indicator bottom margin.
  // Formula: (48 - 24) / 2 - 1 - 2 = 9px.
  std::string get_indicator_bottom_js = base::StringPrintf(
      "window.getComputedStyle("
      "%s.shadowRoot.querySelector('.status-indicator')).bottom",
      GetButtonAppJS(kSplitTabsSelector).c_str());
  EXPECT_EQ(
      "9px",
      content::EvalJs(web_contents, get_indicator_bottom_js).ExtractString());
}
