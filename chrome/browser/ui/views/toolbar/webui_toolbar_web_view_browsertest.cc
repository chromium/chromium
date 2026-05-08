// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_ids.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_pinned_toolbar_actions.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/collaboration/public/features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/data_sharing/public/features.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/ax_inspect_factory.h"
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
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/ax_update_observer.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/menu_runner_test_api.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kNumMaxRecoveryTime = 2;
constexpr base::TimeDelta kRecoveryResetInterval = base::Seconds(10);
constexpr base::TimeDelta kRecoveryRetryInterval = base::Seconds(20);

constexpr char kSplitTabsSelector[] = "split-tabs-button";
constexpr char kReloadButtonSelector[] = "reload-button";
constexpr char kBackSelector[] = "#back";
constexpr char kForwardSelector[] = "#forward";
constexpr char kHomeSelector[] = "#home";

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
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->toolbar_button_provider()
      ->GetWebUIToolbarViewForTesting();
}

void PinButton(Browser* browser, views::WebView* web_view, const char* pref) {
  browser->profile()->GetPrefs()->SetBoolean(pref, true);
  content::WaitForCopyableViewInWebContents(web_view->GetWebContents());
}

bool WaitForButtonEnabled(content::WebContents* web_contents,
                          const std::string& selector) {
  return base::test::RunUntil([&]() {
    return content::EvalJs(web_contents,
                           base::StrCat({GetButtonIconJS(selector),
                                         "?.disabled === false"}))
        .ExtractBool();
  });
}

// Observes accessibility events to capture announcement text.
class AXAnnouncementObserver : public views::AXUpdateObserver {
 public:
  explicit AXAnnouncementObserver(views::AXUpdateNotifier* notifier) {
    observation_.Observe(notifier);
#if BUILDFLAG(IS_MAC)
    recorder_ = content::AXInspectFactory::CreateRecorder(
        content::AXInspectFactory::DefaultPlatformRecorderType(),
        /*manager=*/nullptr, base::GetCurrentProcId());
    recorder_->ListenToEvents(base::BindRepeating(
        &AXAnnouncementObserver::OnMacEvent, base::Unretained(this)));
#endif
  }

  // Waits for the expected announcement to be received. Returns true on
  // success, false on timeout.
  // On macOS, this will only wait for any announcement to be received.
  bool verify_last_announcement(int message_id) {
    bool result = base::test::RunUntil([&]() {
#if BUILDFLAG(IS_MAC)
      return mac_announcement_received_;
#else
      return last_announcement_ == l10n_util::GetStringUTF16(message_id);
#endif
    });
    // Reset after each verification to allow subsequent announcements to be
    // verified correctly.
    last_announcement_.clear();
#if BUILDFLAG(IS_MAC)
    mac_announcement_received_ = false;
#endif

    return result;
  }

 private:
  // views::AXUpdateObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kAlert) {
      ui::AXNodeData node_data;
      view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
      last_announcement_ =
          node_data.GetString16Attribute(ax::mojom::StringAttribute::kName);
    }
  }

#if BUILDFLAG(IS_MAC)
  void OnMacEvent(const std::string& event) {
    if (event.find("AXAnnouncementRequested") != std::string::npos) {
      mac_announcement_received_ = true;
    }
  }

  std::unique_ptr<ui::AXEventRecorder> recorder_;
  bool mac_announcement_received_ = false;
#endif

  std::u16string last_announcement_;
  base::ScopedObservation<views::AXUpdateNotifier, views::AXUpdateObserver>
      observation_{this};
};

constexpr char kGetCoordinatesJS[] =
    "const rect = target.getBoundingClientRect(); "
    "const x = rect.left + rect.width / 2; "
    "const y = rect.top + rect.height / 2; ";

// Adds functions to `target` to mimic pointer capture functions. Note that real
// pointer capture is lost on pointer up, but the returned functions cannot
// handle that, so if that is important for a test, it must manually call
// `releasePointerCapture('*')`.
std::string AddMockPointerCaptureFunctions(const char* target) {
  return base::StringPrintf(
      R"({
        var element = %s;
        var hasCapture = null;
        element.setPointerCapture = (id) => { hasCapture = id; };
        element.hasPointerCapture = (id) => { return id == hasCapture; };
        element.releasePointerCapture = (id) => {
          if (id == hasCapture || id == '*') {
            hasCapture = null;
          }
        };
      })",
      target);
}

// Dispatches an event to a WebUI toolbar button.
// `selector`: The CSS selector for the button element.
// `event_class`: The JS event class (e.g. 'MouseEvent', 'PointerEvent').
// `type`: The event type string (e.g. 'click', 'contextmenu').
// `options`: JS object string for event options (e.g. "detail: 1, button: 2").
std::string DispatchEventScript(const std::string& selector,
                                const std::string& event_class,
                                const std::string& type,
                                const std::string& options = "") {
  return base::StringPrintf(
      "(() => { const target = %s; "
      "if (target) { "
      "  %s"
      "  %s"
      "  target.dispatchEvent(new %s('%s', "
      "  {bubbles: true, cancelable: true, view: window, clientX: x, clientY: "
      "y, "
      "  %s}));"
      "} })();",
      GetButtonIconJS(selector).c_str(), kGetCoordinatesJS,
      AddMockPointerCaptureFunctions("target").c_str(), event_class.c_str(),
      type.c_str(), options.c_str());
}

// Dispatches a pointerup or pointerdown event based on `event`name`.
std::string DispatchPointerEvent(
    const std::string& event_name,
    const std::string& selector,
    const std::string& pointer_type = "mouse",
    const std::string& opts = "detail: 1, button: 0") {
  const std::string el = GetButtonIconJS(selector);
  return base::StringPrintf(
      "(() => { const target = %s; "
      "%s"
      "%s"
      "target.dispatchEvent(new PointerEvent('%s', {bubbles: true, cancelable: "
      "true, view: window, pointerType: '%s', clientX: x, clientY: y, %s})); "
      "})();",
      el.c_str(), kGetCoordinatesJS,
      AddMockPointerCaptureFunctions("target").c_str(), event_name.c_str(),
      pointer_type.c_str(), opts.c_str());
}

// Simulates a full physical click cycle (press + release) using PointerEvents.
std::string DispatchPointerClick(
    const std::string& selector,
    const std::string& pointer_type = "mouse",
    const std::string& opts = "detail: 1, button: 0") {
  const std::string el = GetButtonIconJS(selector);
  return base::StringPrintf(
      "(() => { const target = %s; "
      "%s"
      "%s"
      "target.dispatchEvent(new PointerEvent('pointerdown', {bubbles: true, "
      "cancelable: true, view: window, pointerType: '%s', clientX: x, clientY: "
      "y, "
      "%s}));"
      "target.dispatchEvent(new PointerEvent('pointerup', {bubbles: true, "
      "cancelable: true, view: window, pointerType: '%s', clientX: x, clientY: "
      "y, "
      "%s})); })();",
      el.c_str(), kGetCoordinatesJS,
      AddMockPointerCaptureFunctions("target").c_str(), pointer_type.c_str(),
      opts.c_str(), pointer_type.c_str(), opts.c_str());
}

class NavigationCounter : public content::WebContentsObserver {
 public:
  explicit NavigationCounter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    navigation_count_++;
  }

  // A helper that waits some time and then checks that no navigations occurred.
  void WaitForNoNavigations() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    EXPECT_EQ(navigation_count_, 0u);
  }

  size_t navigation_count() const { return navigation_count_; }

 private:
  size_t navigation_count_ = 0;
};

class TestMenuRunnerHandler : public views::MenuRunnerHandler {
 public:
  explicit TestMenuRunnerHandler(
      base::RepeatingCallback<void(const gfx::Rect&)> callback)
      : callback_(std::move(callback)) {}
  ~TestMenuRunnerHandler() override = default;

  void RunMenuAt(views::Widget* parent,
                 views::MenuButtonController* button_controller,
                 const gfx::Rect& bounds,
                 views::MenuAnchorPosition anchor,
                 ui::mojom::MenuSourceType source_type,
                 int32_t types) override {
    callback_.Run(bounds);
  }

 private:
  base::RepeatingCallback<void(const gfx::Rect&)> callback_;
};

WebUIToolbarWebView* SetUpAndPinHomeButton(Browser* browser) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser);
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinButton(browser, web_view, prefs::kShowHomeButton);
  EXPECT_TRUE(WaitForButtonVisible(web_view->GetWebContents(), kHomeSelector));
  return webui_toolbar_view;
}

}  // namespace

class WebUIToolbarWebViewPixelBrowserTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewPixelBrowserTest() {
    // All features for Webium Production should be included here.
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUISplitTabsButton, features::kWebUIBackForwardButton,
         features::kWebUIHomeButton, features::kWebUIPinnedToolbarActions,
         tabs::kHorizontalTabStripComboButton, features::kWebUILocationBar,
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
                  views::WebView** web_view_out,
                  Browser* browser) {
    // Wait for the WebUIToolbarWebView to be available.
    *webui_toolbar_view_out = nullptr;
    ASSERT_TRUE(base::test::RunUntil([&]() {
      BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser);
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
        *element_out = BrowserElements::From(browser)->GetElement(element_id);
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

  void BasicPixelTest(Browser* browser, const std::string& screenshot_name) {
    ui::TrackedElement* element = nullptr;
    WebUIToolbarWebView* webui_toolbar_view = nullptr;
    views::WebView* web_view = nullptr;
    ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kWebUIToolbarElementIdentifier, &element,
                                       &webui_toolbar_view, &web_view,
                                       browser));

    // Assert that WebContents is not loading, as it affects the state of the
    // reload button.
    ASSERT_FALSE(web_view->GetWebContents()->IsLoading());
    // The WebView should be using the light color mode for regular windows,
    // and dark color mode for incognito windows.
    ASSERT_EQ(web_view->GetWidget()->GetColorMode(),
              browser->profile()->IsIncognitoProfile()
                  ? ui::ColorProviderKey::ColorMode::kDark
                  : ui::ColorProviderKey::ColorMode::kLight);

    // Pixel test
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kVerifyPixels)) {
      views::ViewSkiaGoldPixelDiff pixel_diff(
          "WebUIToolbarWebViewPixelBrowserTest");
      EXPECT_TRUE(pixel_diff.CompareViewScreenshot(screenshot_name,
                                                   webui_toolbar_view));
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/493362471): Re-enable this test.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest, DISABLED_Basic) {
  BasicPixelTest(browser(), "Basic");
}

// TODO(crbug.com/493362471): Re-enable this test.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       DISABLED_IncognitoBasic) {
  BasicPixelTest(CreateIncognitoBrowser(), "IncognitoBasic");
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest, Accessibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kWebUIToolbarElementIdentifier, &element,
                                     &webui_toolbar_view, &web_view,
                                     browser()));

  content::FindAccessibilityNodeCriteria find_criteria;

  // Verify appropriate accessibility properties for back button.
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Back");
  find_criteria.name = "Back";
  ui::AXPlatformNodeDelegate* back_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(back_node);
  const ui::AXNodeData& back = back_node->GetData();
  EXPECT_EQ(ax::mojom::Role::kButton, back.role);
  EXPECT_EQ("Back", back.GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Click to go back, hold to see history",
            back.GetStringAttribute(ax::mojom::StringAttribute::kDescription));

  // Verify appropriate accessibility properties for forward button.
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Forward");
  find_criteria.name = "Forward";
  ui::AXPlatformNodeDelegate* forward_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(forward_node);
  const ui::AXNodeData& forward = forward_node->GetData();
  EXPECT_EQ(ax::mojom::Role::kButton, forward.role);
  EXPECT_EQ("Forward",
            forward.GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(
      "Click to go forward, hold to see history",
      forward.GetStringAttribute(ax::mojom::StringAttribute::kDescription));

  // Verify appropriate accessibility properties for reload button.
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Reload");
  find_criteria.name = "Reload";
  ui::AXPlatformNodeDelegate* reload_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(reload_node);
  const ui::AXNodeData& reload = reload_node->GetData();
  EXPECT_EQ(ax::mojom::Role::kButton, reload.role);
  EXPECT_EQ(true, reload.IsClickable());
  EXPECT_EQ("Reload",
            reload.GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Reload this page", reload.GetStringAttribute(
                                    ax::mojom::StringAttribute::kDescription));
  EXPECT_EQ(static_cast<int>(ax::mojom::HasPopup::kFalse),
            reload.GetIntAttribute(ax::mojom::IntAttribute::kHasPopup));

  auto check_reload_a11y = [&](ax::mojom::HasPopup expected_has_popup,
                               const std::string& expected_description) {
    content::WaitForAccessibilityTreeToChange(web_view->GetWebContents());
    content::WaitForAccessibilityTreeToContainNodeWithName(
        web_view->GetWebContents(), "Reload");
    EXPECT_TRUE(base::test::RunUntil([&]() {
      ui::AXPlatformNodeDelegate* node = content::FindAccessibilityNode(
          web_view->GetWebContents(), find_criteria);
      return node &&
             node->GetData().GetIntAttribute(
                 ax::mojom::IntAttribute::kHasPopup) ==
                 static_cast<int>(expected_has_popup) &&
             node->GetData().GetStringAttribute(
                 ax::mojom::StringAttribute::kDescription) ==
                 expected_description;
    }));
  };

  // Verify enabling devtools is reflected in HasPopup attribute.
  webui_toolbar_view->GetReloadControl()->SetDevToolsStatus(true);
  check_reload_a11y(ax::mojom::HasPopup::kMenu,
                    "Reload this page, hold to see more options");

  // Verify that setting mode to kStop is reflected in HasPopup attribute.
  webui_toolbar_view->GetReloadControl()->ChangeMode(ReloadControl::Mode::kStop,
                                                     true);
  check_reload_a11y(ax::mojom::HasPopup::kFalse, "Stop loading this page");

  // Verify it works when returning to kReload mode.
  webui_toolbar_view->GetReloadControl()->ChangeMode(
      ReloadControl::Mode::kReload, true);
  check_reload_a11y(ax::mojom::HasPopup::kMenu,
                    "Reload this page, hold to see more options");

  // Verify appropriate accessibility properties for home button.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Home");
  find_criteria.name = "Home";
  ui::AXPlatformNodeDelegate* home_node =
      content::FindAccessibilityNode(web_view->GetWebContents(), find_criteria);
  ASSERT_TRUE(home_node);
  const ui::AXNodeData& home = home_node->GetData();
  EXPECT_EQ(ax::mojom::Role::kButton, home.role);
  EXPECT_EQ("Home", home.GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("Open the home page",
            home.GetStringAttribute(ax::mojom::StringAttribute::kDescription));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       CheckReloadButtonColor) {
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kReloadButtonElementId, &element,
                                     &webui_toolbar_view, &web_view,
                                     browser()));

  WebUIReloadControl* reload_control =
      static_cast<WebUIReloadControl*>(webui_toolbar_view->GetReloadControl());
  // Make sure reload icon is showing, which has a hole in the middle whose
  // pixel we'll check to see what the background color is.
  ASSERT_EQ(reload_control->mode_, ReloadControl::Mode::kReload);

  gfx::Rect control_rect = element->GetScreenBounds();
  gfx::Rect view_rect = webui_toolbar_view->GetBoundsInScreen();
  // Wait for the back and forward buttons to finish laying out, which should
  // push the reload button over by at least two buttons width.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    control_rect = element->GetScreenBounds();
    return (control_rect.x() - view_rect.x()) >
           2 * GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  }));

  control_rect.Offset(-view_rect.OffsetFromOrigin());

  // Verify reload button background is transparent when not highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, control_rect) == SK_ColorTRANSPARENT;
  }));

  // Show reload button context menu.
  webui_toolbar_view->GetReloadControl()->SetDevToolsStatus(true);
  webui_toolbar_view->HandleContextMenu(
      toolbar_ui_api::mojom::ContextMenuType::kReload, gfx::RectF(control_rect),
      ui::mojom::MenuSourceType::kMouse);

  // Verify reload button state updates.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_view->GetWebContents(),
                           base::StrCat({GetButtonAppJS(kReloadButtonSelector),
                                         ".state.isContextMenuVisible"}))
        .ExtractBool();
  }));

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
                       CheckBackButtonColor) {
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kToolbarBackButtonElementId, &element,
                                     &webui_toolbar_view, &web_view,
                                     browser()));

  gfx::Rect control_rect = element->GetScreenBounds();
  gfx::Rect view_rect = webui_toolbar_view->GetBoundsInScreen();

  // Wait for the back button to finish laying out.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    control_rect = element->GetScreenBounds();
    return control_rect.width() >=
           GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  }));

  control_rect.Offset(-view_rect.OffsetFromOrigin());

  // Sample a point in the background area (e.g. 5,5 from top-left).
  gfx::Rect background_probe_rect(control_rect.x() + 5, control_rect.y() + 5, 1,
                                  1);

  // Verify back button background is transparent when not highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) ==
           SK_ColorTRANSPARENT;
  }));

  // Show back button context menu.
  webui_toolbar_view->HandleContextMenu(
      toolbar_ui_api::mojom::ContextMenuType::kBack, gfx::RectF(control_rect),
      ui::mojom::MenuSourceType::kMouse);

  // Verify back button state updates.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_view->GetWebContents(),
                           base::StrCat({GetButtonAppJS(kBackSelector),
                                         ".state.isContextMenuVisible"}))
        .ExtractBool();
  }));

  // Verify back button is now highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) !=
           SK_ColorTRANSPARENT;
  }));

  // Close back button context menu.
  webui_toolbar_view->back_control_.menu_runner_->Cancel();

  // Verify back button background returns to transparent.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) ==
           SK_ColorTRANSPARENT;
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       CheckForwardButtonColor) {
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kToolbarForwardButtonElementId, &element,
                                     &webui_toolbar_view, &web_view,
                                     browser()));

  gfx::Rect control_rect = element->GetScreenBounds();
  gfx::Rect view_rect = webui_toolbar_view->GetBoundsInScreen();

  // Wait for the back button to finish laying out, which should
  // push the forward button over by at least one button width.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    control_rect = element->GetScreenBounds();
    return (control_rect.x() - view_rect.x()) >=
           GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  }));

  control_rect.Offset(-view_rect.OffsetFromOrigin());

  // Sample a point in the background area (e.g. 5,5 from top-left).
  gfx::Rect background_probe_rect(control_rect.x() + 5, control_rect.y() + 5, 1,
                                  1);

  // Verify forward button background is transparent when not highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) ==
           SK_ColorTRANSPARENT;
  }));

  // Show forward button context menu.
  webui_toolbar_view->HandleContextMenu(
      toolbar_ui_api::mojom::ContextMenuType::kForward,
      gfx::RectF(control_rect), ui::mojom::MenuSourceType::kMouse);

  // Verify forward button state updates.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_view->GetWebContents(),
                           base::StrCat({GetButtonAppJS(kForwardSelector),
                                         ".state.isContextMenuVisible"}))
        .ExtractBool();
  }));

  // Verify forward button is now highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) !=
           SK_ColorTRANSPARENT;
  }));

  // Close forward button context menu.
  webui_toolbar_view->forward_control_.menu_runner_->Cancel();

  // Verify forward button background returns to transparent.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) ==
           SK_ColorTRANSPARENT;
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       CheckHomeButtonColor) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);

  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kToolbarHomeButtonElementId, &element,
                                     &webui_toolbar_view, &web_view,
                                     browser()));

  WebUIHomeControl* home_control = &webui_toolbar_view->home_control_;

  gfx::Rect control_rect = element->GetScreenBounds();
  gfx::Rect view_rect = webui_toolbar_view->GetBoundsInScreen();
  // Wait for the back, forward, and reload to finish laying out, which should
  // push the home button over by at least three buttons width.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    control_rect = element->GetScreenBounds();
    return (control_rect.x() - view_rect.x()) >
           3 * GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  }));

  control_rect.Offset(-view_rect.OffsetFromOrigin());

  // Sample a point in the background area (e.g. 5,5 from top-left).
  gfx::Rect background_probe_rect(control_rect.x() + 5, control_rect.y() + 5, 1,
                                  1);

  // Verify initial state is transparent.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) ==
           SK_ColorTRANSPARENT;
  }));

  // Show context menu.
  webui_toolbar_view->HandleContextMenu(
      toolbar_ui_api::mojom::ContextMenuType::kHome, gfx::RectF(control_rect),
      ui::mojom::MenuSourceType::kMouse);

  // Verify background is highlighted (NOT transparent).
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) !=
           SK_ColorTRANSPARENT;
  }));

  // Close context menu.
  home_control->menu_runner_->Cancel();

  // Verify home button background returns to transparent.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) ==
           SK_ColorTRANSPARENT;
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       CheckSplitTabsButtonColor) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton, true);

  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kToolbarSplitTabsToolbarButtonElementId,
                                     &element, &webui_toolbar_view, &web_view,
                                     browser()));

  WebUISplitTabsControl* split_tabs_control =
      &webui_toolbar_view->split_tabs_control_;

  gfx::Rect control_rect = element->GetScreenBounds();
  gfx::Rect view_rect = webui_toolbar_view->GetBoundsInScreen();
  // Wait for the back, forward, and reload buttons to finish laying out, which
  // should push the split tabs button over by at least three buttons width.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    control_rect = element->GetScreenBounds();
    return (control_rect.x() - view_rect.x()) >
           3 * GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  }));

  control_rect.Offset(-view_rect.OffsetFromOrigin());

  // Sample a point in the background area (e.g. 5,5 from top-left).
  gfx::Rect background_probe_rect(control_rect.x() + 5, control_rect.y() + 5, 1,
                                  1);

  // Verify initial state is transparent.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetCenterPixelColor(web_view, background_probe_rect) ==
           SK_ColorTRANSPARENT;
  }));

  // Show context menu.
  webui_toolbar_view->HandleContextMenu(
      toolbar_ui_api::mojom::ContextMenuType::kSplitTabsContext,
      gfx::RectF(control_rect), ui::mojom::MenuSourceType::kMouse);

  // Verify split tabs button state updates.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_view->GetWebContents(),
                           base::StrCat({GetButtonAppJS(kSplitTabsSelector),
                                         ".state.isContextMenuVisible"}))
        .ExtractBool();
  }));

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

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       BackForwardButtonsVisibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kWebUIToolbarElementIdentifier, &element,
                                     &webui_toolbar_view, &web_view,
                                     browser()));

  // Check initial state: Buttons should be visible but disabled.
  EXPECT_TRUE(WaitForButtonVisible(web_view->GetWebContents(), kBackSelector));
  EXPECT_EQ("true",
            content::EvalJs(web_view->GetWebContents(),
                            base::StrCat({GetButtonIconJS(kBackSelector),
                                          "?.disabled ? 'true' : 'false'"})));

  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kForwardSelector));
  EXPECT_EQ("true",
            content::EvalJs(web_view->GetWebContents(),
                            base::StrCat({GetButtonIconJS(kForwardSelector),
                                          "?.disabled ? 'true' : 'false'"})));

  // Navigate to enable back button.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));

  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Back");

  EXPECT_TRUE(WaitForButtonVisible(web_view->GetWebContents(), kBackSelector));
  EXPECT_EQ("false",
            content::EvalJs(web_view->GetWebContents(),
                            base::StrCat({GetButtonIconJS(kBackSelector),
                                          "?.disabled ? 'true' : 'false'"})));

  // Forward button should be visible but disabled initially (no forward
  // history).
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kForwardSelector));
  EXPECT_EQ("true",
            content::EvalJs(web_view->GetWebContents(),
                            base::StrCat({GetButtonIconJS(kForwardSelector),
                                          "?.disabled ? 'true' : 'false'"})));

  // Go back to enable forward button.
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);

  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Forward");

  // Check that forward is enabled and back is disabled.
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kForwardSelector));
  EXPECT_EQ("false",
            content::EvalJs(web_view->GetWebContents(),
                            base::StrCat({GetButtonIconJS(kForwardSelector),
                                          "?.disabled ? 'true' : 'false'"})));

  EXPECT_TRUE(WaitForButtonVisible(web_view->GetWebContents(), kBackSelector));
  EXPECT_EQ("true",
            content::EvalJs(web_view->GetWebContents(),
                            base::StrCat({GetButtonIconJS(kBackSelector),
                                          "?.disabled ? 'true' : 'false'"})));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       BackForwardButtonsNavigation) {
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kWebUIToolbarElementIdentifier, &element,
                                     &webui_toolbar_view, &web_view,
                                     browser()));
  PinButton(browser(), web_view, prefs::kShowForwardButton);

  const struct {
    const char* name;
    std::string back_script;
    std::string forward_script;
  } test_cases[] = {
      {"Mouse Click", DispatchPointerClick(kBackSelector),
       DispatchPointerClick(kForwardSelector)},
      {"Keyboard Click",
       DispatchEventScript(kBackSelector, "MouseEvent", "click", "detail: 0"),
       DispatchEventScript(kForwardSelector, "MouseEvent", "click",
                           "detail: 0")}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    // Create navigation history.
    GURL url1("chrome://version/");
    GURL url2("chrome://flags/");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

    // Wait for the back button to be enabled.
    ASSERT_TRUE(
        WaitForButtonEnabled(web_view->GetWebContents(), kBackSelector));

    // Click Back.
    {
      content::TestNavigationObserver nav_observer(
          browser()->tab_strip_model()->GetActiveWebContents());
      EXPECT_TRUE(
          content::ExecJs(web_view->GetWebContents(), test_case.back_script));
      nav_observer.Wait();
      EXPECT_EQ(url1, browser()
                          ->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
    }

    // Wait for the forward button to be enabled.
    ASSERT_TRUE(
        WaitForButtonEnabled(web_view->GetWebContents(), kForwardSelector));

    // Click Forward.
    {
      content::TestNavigationObserver nav_observer(
          browser()->tab_strip_model()->GetActiveWebContents());
      EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(),
                                  test_case.forward_script));
      nav_observer.Wait();
      EXPECT_EQ(url2, browser()
                          ->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
    }
  }
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       BackForwardButtonsModifierClick) {
  ui::TrackedElement* element = nullptr;
  WebUIToolbarWebView* webui_toolbar_view = nullptr;
  views::WebView* web_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(SetUpWebUI(kWebUIToolbarElementIdentifier, &element,
                                     &webui_toolbar_view, &web_view,
                                     browser()));
  PinButton(browser(), web_view, prefs::kShowForwardButton);

  // Create navigation history.
  GURL url1("chrome://version/");
  GURL url2("chrome://flags/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  // Wait for the back button to be enabled.
  ASSERT_TRUE(WaitForButtonEnabled(web_view->GetWebContents(), kBackSelector));

  int initial_tab_count = browser()->tab_strip_model()->count();

#if BUILDFLAG(IS_MAC)
  // Ctrl+Click Back button.
  // On Mac, Ctrl+Click opens a context menu and does not navigate.
  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerClick(kBackSelector, "mouse",
                           "detail: 1, button: 0, ctrlKey: true")));

  auto* back_control = &webui_toolbar_view->back_control_;
  back_control->menu_runner_->Cancel();

  // Verify no new tab was opened.
  EXPECT_EQ(initial_tab_count, browser()->tab_strip_model()->count());
  // Verify we didn't navigate away.
  EXPECT_EQ(url2, browser()
                      ->tab_strip_model()
                      ->GetActiveWebContents()
                      ->GetLastCommittedURL());
#else
  // Ctrl+Click Back button (New Tab).
  content::TestNavigationObserver nav_observer(nullptr);
  nav_observer.StartWatchingNewWebContents();

  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerClick(kBackSelector, "mouse",
                           "detail: 1, button: 0, ctrlKey: true")));

  nav_observer.Wait();

  // Verify new tab was opened.
  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(url1, nav_observer.last_navigation_url());

  // Switch back to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
#endif  // BUILDFLAG(IS_MAC)

  // Navigate back to enable the forward button.
  {
    content::TestNavigationObserver back_nav_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    back_nav_observer.Wait();
  }

  // Wait for the forward button to be enabled.
  ASSERT_TRUE(
      WaitForButtonEnabled(web_view->GetWebContents(), kForwardSelector));

  // Shift+Click Forward button (New Window).
  ui_test_utils::BrowserCreatedObserver new_browser_observer;

  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerClick(kForwardSelector, "mouse",
                           "detail: 1, button: 0, shiftKey: true")));

  Browser* new_browser = new_browser_observer.Wait();
  ASSERT_TRUE(new_browser);

  // Wait for the navigation in the new browser's active tab.
  content::WebContents* new_tab =
      new_browser->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_tab);
  if (new_tab->GetLastCommittedURL() != url2) {
    observer.WaitForNavigationFinished();
  }

  // Verify navigation happened in a new window/web contents.
  EXPECT_EQ(url2, new_tab->GetLastCommittedURL());
}

// Simulate pressing pointer down on the home button, up on the reload button.
// Either button, if clicked, triggers a navigation, but neither button should
// treat this as a click. Since this test moves the pointer horizontally and
// does so instantly, it should not trigger the long press logic.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewPixelBrowserTest,
                       PointerDownOnOneUpOnAnother) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  // Release the pointer over the button.
  NavigationCounter nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::string script = base::StringPrintf(
      R"((() => {
          const home = %s;
          const home_rect = home.getBoundingClientRect();
          const home_x = home_rect.left + home_rect.width / 2;
          const home_y = home_rect.top + home_rect.height / 2;
          // The home button is where the down event occurs, so should be the
          // one with the usual mock pointer functions.
          %s

          const reload = %s;
          const reload_rect = reload.getBoundingClientRect();
          const reload_x = reload_rect.left + reload_rect.width / 2;
          const reload_y = reload_rect.top + reload_rect.height / 2;
          // The reload button should check for pointer capture, but then do
          // nothing, since it doesn't have capture.
          reload.setPointerCapture = () => {
            throw 'setPointerCapture should not be called';
          };
          reload.hasPointerCapture = () => { return false; };
          reload.releasePointerCapture = () => {
            throw 'releasePointerCapture should not be called';
          };

          // Down on the home button.
          home.dispatchEvent(new PointerEvent('pointerdown',
              {bubbles: true, cancelable: true, view: window,
                pointerType: 'mouse', detail: 1, button: 0,
                clientX: home_x, clientY: home_y}));

          // Move to the edge of the home button, and then to the center of the
          // reload button
          home.dispatchEvent(new PointerEvent('pointermove',
              {bubbles: true, cancelable: true, view: window,
                pointerType: 'mouse',
                clientX: home_x + home_rect.width / 2 - 1, clientY: home_y}));
          reload.dispatchEvent(new PointerEvent('pointermove',
              {bubbles: true, cancelable: true, view: window,
                pointerType: 'mouse',
                clientX: reload_x, clientY: reload_y}));

          // Up on the reload button.
          reload.dispatchEvent(new PointerEvent('pointerup',
              {bubbles: true, cancelable: true, view: window,
                pointerType: 'mouse', detail: 1, button: 0,
                clientX: reload_x, clientY: reload_y}));
      })();)",
      GetButtonIconJS(kHomeSelector),
      AddMockPointerCaptureFunctions("home").c_str(),
      GetButtonIconJS(kReloadButtonSelector));
  EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(), script));

  nav_observer.WaitForNoNavigations();
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

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewStabilityTest,
                       ShutdownBrowserBeforeInit) {
  base::SimpleTestTickClock clock;
  WebUIToolbarWebView* toolbar_view = GetWebUIToolbarWebView();
  toolbar_view->SetTickClockForTesting(&clock);

  auto* web_contents = GetWebContents(toolbar_view);
  content::TestNavigationObserver observer(web_contents);
  // Sets the browser window interface on the web content to simulate a
  // browser shutdown.
  webui::SetBrowserWindowInterface(web_contents, nullptr);

  // Force a reload of the web content by killing it.
  KillRendererUntilExceedingLimit(toolbar_view, web_contents);
  clock.Advance(base::Seconds(1) + kRecoveryRetryInterval);

  observer.WaitForNavigationFinished();
  // Succeeded, but not really. Browser should be shutting down at this point
  // so we just have to make sure it doesn't crash.
  ASSERT_TRUE(observer.last_navigation_succeeded());
}

class WebUIToolbarWebViewRaceTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewRaceTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton,
         features::kWebUIInProcessResourceLoadingV2,
         features::kSkipIPCChannelPausingForNonGuests},
        {features::kInitialWebUISyncNavStartToCommit});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for crbug.com/478033216.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewRaceTest,
                       BindInterfaceAfterCloseRace) {
  // 1. Setup: Create a new browser window.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ui_test_utils::WaitForBrowserSetLastActive(new_browser);

  WebUIToolbarWebView* toolbar_view = ::GetWebUIToolbarWebView(new_browser);
  ASSERT_TRUE(toolbar_view);
  content::WebContents* webui_contents =
      toolbar_view->GetWebViewForTesting()->GetWebContents();
  ASSERT_TRUE(webui_contents);

  // 2. Prepare Navigation Manager to hang the navigation.
  GURL toolbar_url(chrome::kChromeUIWebUIToolbarURL);

  // Trigger a reload to start a new navigation that we can control.
  content::TestNavigationManager nav_manager(webui_contents, toolbar_url);
  webui_contents->GetController().Reload(content::ReloadType::NORMAL,
                                         /*check_for_repost=*/false);
  EXPECT_TRUE(nav_manager.WaitForResponse());

  // 3. Resume navigation (this queues the commit task on the UI thread).
  nav_manager.ResumeNavigation();

  // 4. Initiate browser closure.
  // This synchronously calls Browser::OnWindowClosing() which nulls the
  // BrowserWindowInterface reference and posts SynchronouslyDestroyBrowser.
  new_browser->window()->Close();

  // 5. Queue BindInterface manually.
  // This mimics the Mojo request from the renderer arriving after the BWI is
  // nulled but BEFORE the browser is destroyed.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> weak_wc) {
            if (!weak_wc) {
              return;
            }
            auto* rfh = weak_wc->GetPrimaryMainFrame();
            auto* web_ui = rfh ? rfh->GetWebUI() : nullptr;
            auto* ui = web_ui ? web_ui->GetController()->GetAs<WebUIToolbarUI>()
                              : nullptr;
            if (ui) {
              mojo::PendingRemote<tracked_element::mojom::TrackedElementHandler>
                  remote;
              ui->BindInterface(remote.InitWithNewPipeAndPassReceiver());
            }
          },
          webui_contents->GetWeakPtr()));

  // 6. Return to the message loop.
  // This will process: [Commit Task] -> [BindInterface Task] -> [Destruction
  // Task]. Without the fix, both Commit and BindInterface tasks would crash.
  std::ignore = nav_manager.WaitForNavigationFinished();
}

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
  WebUIToolbarWebViewBrowserTest()
      : WebUIToolbarWebViewBrowserTest(
            {features::kInitialWebUI, features::kWebUIReloadButton,
             features::kWebUISplitTabsButton, features::kWebUIHomeButton,
             features::kSkipIPCChannelPausingForNonGuests,
             features::kWebUIInProcessResourceLoadingV2,
             features::kInitialWebUISyncNavStartToCommit},
            {}) {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ThemeServiceFactory::GetForProfile(browser()->profile())
        ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
  }

 protected:
  WebUIToolbarWebViewBrowserTest(
      const std::vector<base::test::FeatureRef>& enabled,
      const std::vector<base::test::FeatureRef>& disabled) {
    feature_list_.InitWithFeatures(enabled, disabled);
  }

  base::test::ScopedFeatureList feature_list_;
};

struct ButtonVisibilityToggleTestParam {
  const char* test_name;
  const char* button_acc_name_key;
  const char* button_pref;
  const char* button_selector;
};

class WebUIToolbarWebViewButtonVisibilityTest
    : public WebUIToolbarWebViewBrowserTest,
      public testing::WithParamInterface<ButtonVisibilityToggleTestParam> {};

IN_PROC_BROWSER_TEST_P(WebUIToolbarWebViewButtonVisibilityTest,
                       TogglesVisibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  const auto& param = GetParam();

  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(webui_toolbar_view);
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);

  // Get button name from WebUI.
  std::string button_name =
      content::EvalJs(
          web_view->GetWebContents(),
          base::StringPrintf(
              "import('//resources/js/load_time_data.js').then(m => "
              "m.loadTimeData.getString('%s'))",
              param.button_acc_name_key))
          .ExtractString();

  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.name = button_name;
  find_criteria.role = ax::mojom::Role::kButton;

  // Wait for a known always-present node to ensure the accessibility tree is
  // populated.
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), "Reload");

  // Verify the button is initially not found.
  EXPECT_FALSE(content::FindAccessibilityNode(web_view->GetWebContents(),
                                              find_criteria));

  // Pin the button and wait for it to become visible.
  PinButton(browser(), web_view, param.button_pref);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), param.button_selector));

  // Wait for it to appear in the accessibility tree.
  content::WaitForAccessibilityTreeToContainNodeWithName(
      web_view->GetWebContents(), button_name);

  // Verify it now exists.
  EXPECT_TRUE(content::FindAccessibilityNode(web_view->GetWebContents(),
                                             find_criteria));

  // Disable the button via pref and wait for the tree to update.
  browser()->profile()->GetPrefs()->SetBoolean(param.button_pref, false);
  content::WaitForAccessibilityTreeToChange(web_view->GetWebContents());

  // Verify it is gone.
  EXPECT_FALSE(content::FindAccessibilityNode(web_view->GetWebContents(),
                                              find_criteria));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebUIToolbarWebViewButtonVisibilityTest,
    testing::Values(
        ButtonVisibilityToggleTestParam{
            .test_name = "SplitTabsButton",
            .button_acc_name_key = "splitTabsButtonAccNamePinned",
            .button_pref = prefs::kPinSplitTabButton,
            .button_selector = kSplitTabsSelector},
        ButtonVisibilityToggleTestParam{
            .test_name = "HomeButton",
            .button_acc_name_key = "homeButtonAccName",
            .button_pref = prefs::kShowHomeButton,
            .button_selector = kHomeSelector}),
    [](const testing::TestParamInfo<ButtonVisibilityToggleTestParam>& info) {
      // Use the custom test name for clarity in test results.
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewBrowserTest,
                       VerifyDynamicTouchModeUpdate) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  PinButton(browser(), web_view, prefs::kPinSplitTabButton);
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

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewBrowserTest,
                       DISABLED_ParseFinishedToFirstUpdateMetrics) {
  base::HistogramTester histogram_tester;

  // Open a new one to capture the initial metric if it was already recorded.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ui_test_utils::WaitForBrowserSetLastActive(new_browser);
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(new_browser);
  ASSERT_TRUE(webui_toolbar_view);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectTotalCount(
      "InitialWebUI.Toolbar.ParseFinishedToFirstUpdate", 1);
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewBrowserTest, ContextMenuPositionE2E) {
  // Setup the WebUI and wait for it to render.
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  ASSERT_TRUE(webui_toolbar_view);
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  EXPECT_TRUE(WaitForButtonVisible(web_contents, kReloadButtonSelector));

  // Enable the context menu (requires devtools to be open).
  WebUIReloadControl* reload_control =
      static_cast<WebUIReloadControl*>(webui_toolbar_view->GetReloadControl());
  reload_control->SetDevToolsStatus(true);

  // Intercept the C++ MenuRunner to capture the bounds it receives.
  // We use a RunLoop because the Mojo IPC from the WebUI is asynchronous.
  gfx::Rect captured_bounds;
  base::RunLoop run_loop;
  auto handler = std::make_unique<TestMenuRunnerHandler>(
      base::BindLambdaForTesting([&](const gfx::Rect& bounds) {
        captured_bounds = bounds;
        run_loop.Quit();
      }));

  views::test::MenuRunnerTestAPI(reload_control->menu_runner_.get())
      .SetMenuRunnerHandler(std::move(handler));

  // Ask JavaScript for the real bounding rect of the button.
  content::EvalJsResult result = content::EvalJs(
      web_contents, base::StrCat({GetButtonAppJS(kReloadButtonSelector),
                                  ".getBoundingClientRect().toJSON()"}));
  ASSERT_TRUE(result.is_ok());
  const base::DictValue& css_bounds = result.ExtractDict();

  std::optional<double> left = css_bounds.FindDouble("left");
  std::optional<double> top = css_bounds.FindDouble("top");
  std::optional<double> width = css_bounds.FindDouble("width");
  std::optional<double> height = css_bounds.FindDouble("height");

  ASSERT_TRUE(left.has_value()) << "left missing";
  ASSERT_TRUE(top.has_value()) << "top missing";
  ASSERT_TRUE(width.has_value()) << "width missing";
  ASSERT_TRUE(height.has_value()) << "height missing";

  // Calculate what the C++ screen coordinates should be.
  double page_zoom_scale = blink::ZoomLevelToZoomFactor(
      zoom::ZoomController::GetZoomLevelForWebContents(web_contents));
  gfx::Rect expected_screen_bounds = gfx::ToEnclosingRect(gfx::ScaleRect(
      gfx::RectF(*left, *top, *width, *height), page_zoom_scale));
  expected_screen_bounds.Offset(
      webui_toolbar_view->GetBoundsInScreen().origin().OffsetFromOrigin());

  // Trigger the context menu via a real DOM Right-Click event.
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      base::StringPrintf(
          "%s?.dispatchEvent(new MouseEvent('contextmenu', "
          "{bubbles: true, cancelable: true, view: window, button: 2}));",
          GetButtonIconJS(kReloadButtonSelector).c_str())));

  // Wait for the Mojo IPC to reach C++ and trigger the menu runner.
  run_loop.Run();

  EXPECT_EQ(captured_bounds, expected_screen_bounds);
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

IN_PROC_BROWSER_TEST_F(WebUIReloadButtonBrowserTest, ClickReloadButton) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* webui_contents = web_view->GetWebContents();

  EXPECT_TRUE(WaitForButtonVisible(webui_contents, kReloadButtonSelector));

  const struct {
    const char* name;
    std::string script;
  } test_cases[] = {
      {"Mouse Click", DispatchPointerClick(kReloadButtonSelector)},
      {"Keyboard Click",
       DispatchEventScript(kReloadButtonSelector, "MouseEvent", "click",
                           "detail: 0")}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    const std::string& script = test_case.script;
    // Navigate to a known good URL before reloading to ensure a clean state
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));

    // Create a navigation observer on the active tab.
    content::WebContents* active_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver nav_observer(active_contents);

    EXPECT_TRUE(content::ExecJs(webui_contents, script));

    nav_observer.Wait();
    // If the navigation happened, it means the reload button was activated.
  }
}

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
       DispatchEventScript(kSplitTabsSelector, "MouseEvent", "click",
                           "detail: 0"),
       ui::mojom::MenuSourceType::kKeyboard},
      {"Mouse Click", DispatchPointerClick(kSplitTabsSelector),
       ui::mojom::MenuSourceType::kMouse},
      {"Touch Click", DispatchPointerClick(kSplitTabsSelector, "touch"),
       ui::mojom::MenuSourceType::kTouch},
      {"Pen Click", DispatchPointerClick(kSplitTabsSelector, "pen"),
       ui::mojom::MenuSourceType::kTouch},
      {"Keyboard Context Menu",
       DispatchEventScript(kSplitTabsSelector, "MouseEvent", "contextmenu",
                           "detail: 0"),
       ui::mojom::MenuSourceType::kKeyboard},
      {"Mouse Context Menu",
       DispatchEventScript(kSplitTabsSelector, "MouseEvent", "contextmenu",
                           "button: 2"),
       ui::mojom::MenuSourceType::kMouse},
      {"Touch Context Menu",
       DispatchEventScript(kSplitTabsSelector, "PointerEvent", "contextmenu",
                           "pointerType: 'touch'"),
       ui::mojom::MenuSourceType::kTouch},
      {"Pen Context Menu",
       DispatchEventScript(kSplitTabsSelector, "PointerEvent", "contextmenu",
                           "pointerType: 'pen'"),
       ui::mojom::MenuSourceType::kTouch},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);
    EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(), test_case.script));
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return split_tabs_control->last_source_type_for_testing_ !=
             ui::mojom::MenuSourceType::kNone;
    }));
    EXPECT_EQ(test_case.expected_source,
              split_tabs_control->last_source_type_for_testing_);
    split_tabs_control->menu_runner_->Cancel();
    // Reset last_source_type_for_testing_ to kNone to ensure the next
    // iteration correctly checks for a new value instead of preserving
    // the old one.
    split_tabs_control->last_source_type_for_testing_ =
        ui::mojom::MenuSourceType::kNone;
  }
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       ClickSplitTabsButton) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinButton(browser(), web_view, prefs::kPinSplitTabButton);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));

  auto* tab_strip_model = browser()->tab_strip_model();

  const struct {
    const char* name;
    std::string script;
    std::string cleanup_script;
  } test_cases[] = {
      {"Mouse Click", DispatchPointerEvent("pointerdown", kSplitTabsSelector),
       DispatchPointerEvent("pointerup", kSplitTabsSelector)},
      {"Keyboard Click",
       DispatchEventScript(kSplitTabsSelector, "MouseEvent", "click",
                           "detail: 0"),
       ""}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    // Ensure NOT in split view initially by opening a new tab.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("chrome://version/"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    EXPECT_FALSE(tab_strip_model->GetActiveTab()->IsSplit());

    EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(), test_case.script));

    // Verify entered split view. This might take a moment, so need to wait.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return tab_strip_model->GetActiveTab()->IsSplit(); }));

    if (!test_case.cleanup_script.empty()) {
      // Dispatch cleanup script to complete the interaction cleanly.
      EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(),
                                  test_case.cleanup_script));
    }
  }
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       SplitTabsButtonAriaHasPopup) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  PinButton(browser(), web_view, prefs::kPinSplitTabButton);
  ASSERT_TRUE(WaitForButtonVisible(web_contents, kSplitTabsSelector));

  // Initially NOT split. aria-haspopup should be 'false'.
  const std::string kGetAriaHasPopup =
      base::StrCat({GetButtonIconJS(kSplitTabsSelector),
                    "?.getAttribute('aria-haspopup') || 'false'"});
  EXPECT_EQ("false", content::EvalJs(web_contents, kGetAriaHasPopup));
  EXPECT_TRUE(
      content::ExecJs(web_contents, DispatchPointerClick(kSplitTabsSelector)));

  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_model->GetActiveTab()->IsSplit(); }));

  // Now split. aria-haspopup should be 'true'.
  // The state update is async from the browser back to the WebUI.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(web_contents, kGetAriaHasPopup).ExtractString() ==
           "true";
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       RightClickSplitTabsButton) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinButton(browser(), web_view, prefs::kPinSplitTabButton);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));
  EXPECT_TRUE(
      content::ExecJs(web_view->GetWebContents(),
                      DispatchEventScript(kSplitTabsSelector, "MouseEvent",
                                          "contextmenu", "button: 2")));

  WebUISplitTabsControl* split_tabs_control =
      &webui_toolbar_view->split_tabs_control_;

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return split_tabs_control->menu_runner_ &&
           split_tabs_control->menu_runner_->IsRunning();
  }));

  // Clean up
  split_tabs_control->menu_runner_->Cancel();
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       ClickSplitTabsButtonWhileSplit) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinButton(browser(), web_view, prefs::kPinSplitTabButton);
  EXPECT_TRUE(
      WaitForButtonVisible(web_view->GetWebContents(), kSplitTabsSelector));

  // Create a split tab group manually to simulate being in split mode.
  chrome::NewSplitTab(browser(),
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return tab_strip_model->GetActiveTab()->IsSplit(); }));

  // Click the button while in split mode using ONLY pointerdown.
  EXPECT_TRUE(
      content::ExecJs(web_view->GetWebContents(),
                      DispatchPointerEvent("pointerdown", kSplitTabsSelector)));

  // Verify no crash by ensuring we can cleanly dispatch the pointerup.
  EXPECT_TRUE(
      content::ExecJs(web_view->GetWebContents(),
                      DispatchPointerEvent("pointerup", kSplitTabsSelector)));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewSplitTabsBrowserTest,
                       VerifySplitTabLocations) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  PinButton(browser(), web_view, prefs::kPinSplitTabButton);
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

  PinButton(browser(), web_view, prefs::kPinSplitTabButton);
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

// Tests for the home button. Also serve as the general PressHandler tests.
class WebUIToolbarWebViewHomeButtonBrowserTest : public InProcessBrowserTest {
 public:
  WebUIToolbarWebViewHomeButtonBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIHomeButton,
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

 protected:
  void WaitForUndoBubble(WebUIToolbarWebView* webui_toolbar_view) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                 HomePageUndoBubbleCoordinator::kHomePageUndoBubbleMainViewId,
                 views::ElementTrackerViews::GetContextForView(
                     webui_toolbar_view)) != nullptr;
    }));
  }

  GURL GetHomeURL() {
    GURL home_url(
        browser()->profile()->GetPrefs()->GetString(prefs::kHomePage));
    if (home_url.is_empty()) {
      return chrome::ChromeUINewTabURLAsGURL();
    }
    return home_url;
  }

  WebUIToolbarWebView* PerformDragAndDrop(const std::string& new_home_url) {
    WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
    views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
    content::WebContents* web_contents = web_view->GetWebContents();

    // JS to simulate a drop event on the home button.
    EXPECT_TRUE(content::ExecJs(
        web_contents,
        base::StringPrintf(R"(
      const homeButton = document.querySelector('toolbar-app').shadowRoot
                             .querySelector('#home').shadowRoot
                             .querySelector('cr-icon-button');
      const dataTransfer = new DataTransfer();
      dataTransfer.setData('text/uri-list', '%s');
      dataTransfer.setData('text/plain', '%s');
      const dropEvent = new DragEvent('drop', {
        bubbles: true,
        cancelable: true,
        dataTransfer: dataTransfer
      });
      homeButton.dispatchEvent(dropEvent);
    )",
                           new_home_url.c_str(), new_home_url.c_str())));

    // Wait for the bubble widget to be created.
    WaitForUndoBubble(webui_toolbar_view);

    // Verify the new home page was correctly set.
    auto* prefs = browser()->profile()->GetPrefs();
    EXPECT_EQ(new_home_url, prefs->GetString(prefs::kHomePage));
    EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));

    return webui_toolbar_view;
  }

  void PerformUndo(WebUIToolbarWebView* webui_toolbar_view) {
    // Click undo.
    auto* bubble =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            HomePageUndoBubbleCoordinator::kHomePageUndoBubbleMainViewId,
            views::ElementTrackerViews::GetContextForView(webui_toolbar_view));
    ASSERT_TRUE(bubble);
    auto* styled_label =
        static_cast<views::StyledLabel*>(bubble->children().front());
    styled_label->ClickFirstLinkForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       ClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  const struct {
    const char* name;
    std::string script;
  } test_cases[] = {
      {"Mouse Click", DispatchPointerClick(kHomeSelector)},
      {"Keyboard Click",
       DispatchEventScript(kHomeSelector, "MouseEvent", "click", "detail: 0")}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    const std::string& script = test_case.script;
    // Navigate away so clicking home actually does something.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://version")));

    // Click the button.
    content::TestNavigationObserver nav_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(), script));
    nav_observer.Wait();

    EXPECT_EQ(home_url, browser()
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetLastCommittedURL());
  }
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       RightClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(),
                              DispatchEventScript(kHomeSelector, "MouseEvent",
                                                  "contextmenu", "button: 2")));

  WebUIHomeControl* home_control = &webui_toolbar_view->home_control_;

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return home_control->menu_runner_ &&
           home_control->menu_runner_->IsRunning();
  }));

  // Clean up
  home_control->menu_runner_->Cancel();
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       LongPressHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(),
                              DispatchEventScript(kHomeSelector, "PointerEvent",
                                                  "pointerdown", "button: 0")));

  WebUIHomeControl* home_control = &webui_toolbar_view->home_control_;

  // Wait for the long press timer to trigger and show the menu.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return home_control->menu_runner_ &&
           home_control->menu_runner_->IsRunning();
  }));

  // Clean up
  home_control->menu_runner_->Cancel();
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       CtrlClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  int initial_tab_count = browser()->tab_strip_model()->count();
  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());

#if BUILDFLAG(IS_MAC)
  const char* kModifier = "metaKey: true";
#else
  const char* kModifier = "ctrlKey: true";
#endif

  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerClick(
          kHomeSelector, "mouse",
          base::StrCat({"detail: 1, button: 0, ", kModifier}))));

  tab_add_waiter.Wait();

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  // Verify new tab is in the background.
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetWebContentsAt(initial_tab_count);
  content::TestNavigationObserver observer(new_tab);
  if (new_tab->GetLastCommittedURL() != home_url) {
    observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(home_url, new_tab->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       CtrlShiftClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  int initial_tab_count = browser()->tab_strip_model()->count();
  ui_test_utils::TabAddedWaiter tab_add_waiter(browser());

#if BUILDFLAG(IS_MAC)
  const char* kModifier = "metaKey: true";
#else
  const char* kModifier = "ctrlKey: true";
#endif

  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerClick(kHomeSelector, "mouse",
                           base::StrCat({"detail: 1, button: 0, ", kModifier,
                                         ", shiftKey: true"}))));

  tab_add_waiter.Wait();

  EXPECT_EQ(initial_tab_count + 1, browser()->tab_strip_model()->count());
  // Verify new tab is in the foreground.
  EXPECT_EQ(initial_tab_count, browser()->tab_strip_model()->active_index());

  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_tab);
  if (new_tab->GetLastCommittedURL() != home_url) {
    observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(home_url, new_tab->GetLastCommittedURL());
}

// Test the case the mouse is released over the home button without pressing on
// it.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       ReleaseOnHomeButtonWithoutPress) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  // Navigate away so clicking home actually does something.
  GURL other_url("chrome://version");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), other_url));

  // Release the pointer over the button.
  NavigationCounter nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  std::string script = base::StringPrintf(
      R"((() => {
          const target = %s;
          %s
          %s
          // Up event with no matching down event.
          target.dispatchEvent(new PointerEvent('pointerup',
              {bubbles: true, cancelable: true, view: window,
                pointerType: 'mouse', clientX: x, clientY: y,
                detail: 1, button: 0}));
      })();)",
      GetButtonIconJS(kHomeSelector), kGetCoordinatesJS,
      AddMockPointerCaptureFunctions("target").c_str());
  EXPECT_TRUE(content::ExecJs(web_view->GetWebContents(), script));

  nav_observer.WaitForNoNavigations();

  EXPECT_EQ(other_url, browser()
                           ->tab_strip_model()
                           ->GetActiveWebContents()
                           ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       TouchModeChangesIcon) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  std::string get_icon_js =
      base::StrCat({"window.getComputedStyle(", GetButtonIconJS(kHomeSelector),
                    ").getPropertyValue('--cr-icon-image')"});

  // Verify standard mode icon
  EXPECT_TRUE(content::EvalJs(web_contents, get_icon_js)
                  .ExtractString()
                  .find("home_20.svg") != std::string::npos);

  {
    ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper(true);

    // Wait and verify Touch mode icon
    EXPECT_TRUE(base::test::RunUntil([&]() {
      std::string current_icon =
          content::EvalJs(web_contents, get_icon_js).ExtractString();
      return current_icon.find("home_24.svg") != std::string::npos;
    }));
  }

  // Revert to non-touch mode happens automatically when scoper goes out of
  // scope

  // Wait and verify standard mode icon again
  EXPECT_TRUE(base::test::RunUntil([&]() {
    std::string current_icon =
        content::EvalJs(web_contents, get_icon_js).ExtractString();
    return current_icon.find("home_20.svg") != std::string::npos;
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       ShiftClickHomeButton) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  GURL home_url = GetHomeURL();

  ui_test_utils::BrowserCreatedObserver new_browser_observer;

  EXPECT_TRUE(content::ExecJs(
      web_view->GetWebContents(),
      DispatchPointerClick(kHomeSelector, "mouse",
                           "detail: 1, button: 0, shiftKey: true")));

  Browser* new_browser = new_browser_observer.Wait();
  ASSERT_TRUE(new_browser);

  content::WebContents* new_tab =
      new_browser->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(new_tab);
  if (new_tab->GetLastCommittedURL() != home_url) {
    observer.WaitForNavigationFinished();
  }
  EXPECT_EQ(home_url, new_tab->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       DragAndDropHomeButton) {
  std::string current_home_url =
      browser()->profile()->GetPrefs()->GetString(prefs::kHomePage);
  std::string new_home_url = "https://www.example.test/";
  EXPECT_NE(current_home_url, new_home_url);

  PerformDragAndDrop(new_home_url);
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       DragAndDropHomeButtonAndUndo) {
  auto* const prefs = browser()->profile()->GetPrefs();
  prefs->SetString(prefs::kHomePage, "https://www.url-a.test");
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
  base::RunLoop().RunUntilIdle();

  WebUIToolbarWebView* webui_toolbar_view =
      PerformDragAndDrop("https://www.url-b.test/");
  PerformUndo(webui_toolbar_view);

  // Verify the home page is reverted.
  EXPECT_EQ("https://www.url-a.test/", prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       DragAndDropHomeButtonAndUndoFromNTP) {
  auto* const prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, true);
  base::RunLoop().RunUntilIdle();

  WebUIToolbarWebView* webui_toolbar_view =
      PerformDragAndDrop("https://www.example.test/");
  PerformUndo(webui_toolbar_view);

  // Verify the home page is reverted.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

// Verify that dropping a file on the home button sets it as the home page,
// and the action can be undone.
IN_PROC_BROWSER_TEST_F(WebUIToolbarWebViewHomeButtonBrowserTest,
                       DropFileOnHomeButtonAndUndo) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  content::WebContents* web_contents =
      webui_toolbar_view->GetWebViewForTesting()->GetWebContents();

  std::string file_path = "/fake/path/to/file.pdf";

  // Get the coordinates of the home button and dispatch event via hit-testing.
  gfx::Point center = BrowserElements::From(browser())
                          ->GetElement(kToolbarHomeButtonElementId)
                          ->GetScreenBounds()
                          .CenterPoint();
  gfx::Point click_point =
      center - webui_toolbar_view->GetBoundsInScreen().OffsetFromOrigin();

  PrefService* prefs = browser()->profile()->GetPrefs();
  GURL old_url = GURL(prefs->GetString(prefs::kHomePage));
  bool old_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);

  content::DropData drop_data;
  drop_data.filenames.emplace_back(base::FilePath::FromUTF8Unsafe(file_path),
                                   base::FilePath());

  webui_toolbar_view->GetWebViewForTesting()
      ->GetWebContents()
      ->GetDelegate()
      ->PreHandleDragUpdate(drop_data, gfx::PointF(click_point));

  // Now actually dispatch the drop event.
  EXPECT_EQ("success",
            content::EvalJs(web_contents, base::StringPrintf(R"(
    (function() {
      const target = document.querySelector('toolbar-app').shadowRoot
                       .querySelector('#home').shadowRoot
                       .querySelector('cr-icon-button');
      const dataTransfer = new DataTransfer();
      Object.defineProperty(dataTransfer, 'types', {value: ['Files']});
      const dropEvent = new DragEvent('drop', {
        bubbles: true,
        cancelable: true,
        clientX: %d,
        clientY: %d,
        dataTransfer: dataTransfer
      });
      target.dispatchEvent(dropEvent);
      return 'success';
    })();
  )",
                                                             click_point.x(),
                                                             click_point.y())));

  // Wait for the undo bubble. This proves the Mojo call reached C++.
  WaitForUndoBubble(webui_toolbar_view);

  GURL expected_url =
      net::FilePathToFileURL(base::FilePath::FromUTF8Unsafe(file_path));
  EXPECT_EQ(prefs->GetString(prefs::kHomePage), expected_url.spec());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));

  PerformUndo(webui_toolbar_view);

  // Verify that the pref is restored.
  EXPECT_EQ(prefs->GetString(prefs::kHomePage), old_url.spec());
  EXPECT_EQ(prefs->GetBoolean(prefs::kHomePageIsNewTabPage), old_is_ntp);
}

class WebUIPinnedToolbarActionsBrowserTest
    : public WebUIToolbarWebViewBrowserTest {
 public:
  WebUIPinnedToolbarActionsBrowserTest()
      : WebUIToolbarWebViewBrowserTest(
            {features::kInitialWebUI, features::kWebUIPinnedToolbarActions,
             features::kSkipIPCChannelPausingForNonGuests,
             features::kWebUIInProcessResourceLoadingV2,
             features::kInitialWebUISyncNavStartToCommit,
             tabs::kHorizontalTabStripComboButton,
             // Facilitate testing kActionSidePanelShowComments
             collaboration::features::kCollaborationComments,
             // Facilitate testing kActionsSidePanelShowContextualTasks
             contextual_tasks::kContextualTasks,
             // Facilitate testing kActionSendSharedTabGroupFeedback
             data_sharing::features::kDataSharingFeature},
            {}) {}

  void SetUpOnMainThread() override {
    WebUIToolbarWebViewBrowserTest::SetUpOnMainThread();
    // Make everything pinnable by default to facilitate testing.
    for (const auto& mapping : kActionMappings) {
      SetPinnableProperty(mapping.first, true);
    }
    model_ = PinnedToolbarActionsModel::Get(browser()->profile());
  }

  void TearDownOnMainThread() override {
    model_ = nullptr;
    WebUIToolbarWebViewBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::EvalJsResult EvalJsOnPinnedButton(
      content::WebContents* web_contents,
      toolbar_ui_api::mojom::PinnedToolbarAction action,
      const std::string& script_body) {
    return content::EvalJs(
        web_contents,
        base::StringPrintf(R"(
      (() => {
        const container = %s?.shadowRoot;
        if (!container) return false;
        const actionEl = Array.from(container.querySelectorAll(
                                 'pinned-toolbar-action'))
                        .find(el => el.state && el.state.action === %d);
        if (!actionEl) return false;
        const btn = actionEl.shadowRoot.querySelector('cr-icon-button');
        %s
      })();
    )",
                           GetButtonAppJS("#pinnedToolbarActions").c_str(),
                           static_cast<int>(action), script_body.c_str()));
  }

  bool IsPinnedButtonVisible(
      content::WebContents* web_contents,
      toolbar_ui_api::mojom::PinnedToolbarAction action) {
    return EvalJsOnPinnedButton(web_contents, action,
                                "return !!btn && btn.checkVisibility();")
        .ExtractBool();
  }

  bool ClickPinnedButton(content::WebContents* web_contents,
                         toolbar_ui_api::mojom::PinnedToolbarAction action) {
    return EvalJsOnPinnedButton(
               web_contents, action,
               "if (!btn || !btn.checkVisibility()) return false; btn.click(); "
               "return true;")
        .ExtractBool();
  }

  void SetPinnableProperty(actions::ActionId id, bool pinnable) {
    actions::ActionManager::Get()
        .FindAction(id, browser()->GetActions()->root_action_item())
        ->SetProperty(
            actions::kActionItemPinnableKey,
            static_cast<int>(pinnable
                                 ? actions::ActionPinnableState::kPinnable
                                 : actions::ActionPinnableState::kNotPinnable));
  }

  views::View* GetLocationBarView() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

  views::BubbleAnchor GetToolbarBubbleAnchor(actions::ActionId action_id) {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetBubbleAnchor(action_id);
  }

  void PinAction(actions::ActionId action_id,
                 toolbar_ui_api::mojom::PinnedToolbarAction mojom_action) {
    auto* webui_toolbar_view = GetWebUIToolbarWebView(browser());
    auto* web_contents =
        webui_toolbar_view->GetWebViewForTesting()->GetWebContents();
    auto* pinned_actions = webui_toolbar_view->GetPinnedToolbarActions();
    ui::ElementIdentifier id =
        pinned_toolbar_actions::GetElementIdentifierForAction(action_id);

    // Verify it's not pinned initially.
    if (id) {
      CHECK_EQ(id, webui_toolbar::ActionIdToElementIdentifier(action_id));
      EXPECT_FALSE(BrowserElements::From(browser())->GetElement(id));
    }
    EXPECT_TRUE(pinned_actions->GetBubbleAnchor(action_id).IsNull());
    bool missing_anchor = false;
    pinned_actions->GetBubbleAnchorAsync(
        action_id, base::BindLambdaForTesting(
                       [&](base::expected<views::BubbleAnchor,
                                          GetAnchorFailureReason> anchor) {
                         EXPECT_FALSE(anchor.has_value());
                         EXPECT_EQ(anchor.error(),
                                   GetAnchorFailureReason::kAnchorNotFound);
                         missing_anchor = true;
                       }));
    EXPECT_TRUE(missing_anchor);
    EXPECT_EQ(GetToolbarBubbleAnchor(action_id).GetIfView(),
              GetLocationBarView());

    model_->UpdatePinnedState(action_id, true);
    // Test async anchor fetching.
    base::RunLoop run_loop;
    pinned_actions->GetBubbleAnchorAsync(
        action_id, base::BindLambdaForTesting(
                       [&](base::expected<views::BubbleAnchor,
                                          GetAnchorFailureReason> anchor) {
                         EXPECT_TRUE(anchor.has_value());
                         EXPECT_FALSE(anchor.value().IsNull());
                         run_loop.Quit();
                       }));
    run_loop.Run();
    // Test sync anchor fetching.
    EXPECT_FALSE(pinned_actions->GetBubbleAnchor(action_id).IsNull());
    EXPECT_TRUE(GetToolbarBubbleAnchor(action_id).GetIfElement());
    bool found_anchor = false;
    pinned_actions->GetBubbleAnchorAsync(
        action_id, base::BindLambdaForTesting(
                       [&](base::expected<views::BubbleAnchor,
                                          GetAnchorFailureReason> anchor) {
                         EXPECT_TRUE(anchor.has_value());
                         EXPECT_FALSE(anchor.value().IsNull());
                         found_anchor = true;
                       }));
    EXPECT_TRUE(found_anchor);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

    // Verify it's not highlighted.
    EXPECT_TRUE(EvalJsOnPinnedButton(web_contents, mojom_action,
                                     "return !!btn && "
                                     "!btn.hasAttribute('is-menu-open');")
                    .ExtractBool());

    // Verify it's trackable.
    if (id) {
      EXPECT_TRUE(base::test::RunUntil([&]() {
        return BrowserElements::From(browser())->GetElement(id) != nullptr;
      }));
    }
  }

  void UnpinAction(actions::ActionId action_id,
                   toolbar_ui_api::mojom::PinnedToolbarAction mojom_action) {
    auto* webui_toolbar_view = GetWebUIToolbarWebView(browser());
    auto* web_contents =
        webui_toolbar_view->GetWebViewForTesting()->GetWebContents();
    auto* pinned_actions = webui_toolbar_view->GetPinnedToolbarActions();
    ui::ElementIdentifier id =
        pinned_toolbar_actions::GetElementIdentifierForAction(action_id);

    model_->UpdatePinnedState(action_id, false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));

    if (id) {
      EXPECT_TRUE(base::test::RunUntil([&]() {
        return BrowserElements::From(browser())->GetElement(id) == nullptr;
      }));
    }
    EXPECT_TRUE(pinned_actions->GetBubbleAnchor(action_id).IsNull());
    EXPECT_EQ(GetToolbarBubbleAnchor(action_id).GetIfView(),
              GetLocationBarView());
  }

  void VerifyPinnedToolbarWidth() {
    WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
    views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
    content::WebContents* web_contents = web_view->GetWebContents();
    auto* pinned_actions = static_cast<WebUIPinnedToolbarActions*>(
        webui_toolbar_view->GetPinnedToolbarActions());

    // Verify HTML element width matches C++ calculated width.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(
                 web_contents,
                 base::StringPrintf(
                     R"(
        (() => {
          const el = %s;
          return el ? el.getBoundingClientRect().width : -1;
        })();
      )",
                     GetButtonAppJS("#pinnedToolbarActions").c_str()))
                 .ExtractInt() == pinned_actions->GetWidth();
    }));
  }

  raw_ptr<PinnedToolbarActionsModel> model_;

  const std::vector<
      std::pair<actions::ActionId, toolbar_ui_api::mojom::PinnedToolbarAction>>
      kActionMappings = {
          {kActionNewIncognitoWindow,
           toolbar_ui_api::mojom::PinnedToolbarAction::kNewIncognitoWindow},
          {kActionShowPasswordsBubbleOrPage,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kShowPasswordsBubbleOrPage},
          {kActionShowPaymentsBubbleOrPage,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kShowPaymentsBubbleOrPage},
          {kActionShowAddressesBubbleOrPage,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kShowAddressesBubbleOrPage},
          {kActionSidePanelShowBookmarks,
           toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowBookmarks},
          {kActionSidePanelShowReadingList,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowReadingList},
          {kActionSidePanelShowHistoryCluster,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowHistoryCluster},
// ChromeOS doesn't support download button.
#if !BUILDFLAG(IS_CHROMEOS)
          {kActionShowDownloads,
           toolbar_ui_api::mojom::PinnedToolbarAction::kShowDownloads},
#endif  // !BUILDFLAG(IS_CHROMEOS)
          {kActionClearBrowsingData,
           toolbar_ui_api::mojom::PinnedToolbarAction::kClearBrowsingData},
          {kActionPrint, toolbar_ui_api::mojom::PinnedToolbarAction::kPrint},
          {kActionSidePanelShowLensOverlayResults,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowLensOverlayResults},
          {kActionShowTranslate,
           toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate},
          {kActionQrCodeGenerator,
           toolbar_ui_api::mojom::PinnedToolbarAction::kQrCodeGenerator},
          {kActionRouteMedia,
           toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMediaIdle},
          {kActionSidePanelShowReadAnything,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowReadAnything},
          {kActionCopyUrl,
           toolbar_ui_api::mojom::PinnedToolbarAction::kCopyUrl},
          {kActionSendTabToSelf,
           toolbar_ui_api::mojom::PinnedToolbarAction::kSendTabToSelf},
          {kActionTaskManager,
           toolbar_ui_api::mojom::PinnedToolbarAction::kTaskManager},
          {kActionDevTools,
           toolbar_ui_api::mojom::PinnedToolbarAction::kDevTools},
          {kActionSidePanelShowContextualTasks,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowContextualTasks},
          {kActionSidePanelShowLens,
           toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowLens},
          {kActionSidePanelShowAboutThisSite,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowAboutThisSite},
          {kActionSidePanelShowCustomizeChrome,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowCustomizeChrome},
          {kActionSidePanelShowShoppingInsights,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowShoppingInsights},
          {kActionSidePanelShowMerchantTrust,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSidePanelShowMerchantTrust},
          {kActionSendSharedTabGroupFeedback,
           toolbar_ui_api::mojom::PinnedToolbarAction::
               kSendSharedTabGroupFeedback},
          {kActionSidePanelShowComments,
           toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowComments},
      };
};

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       PinUnpinIndividually) {
  for (const auto& [action_id, mojom_action] : kActionMappings) {
    PinAction(action_id, mojom_action);
    UnpinAction(action_id, mojom_action);
  }
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, PinAllTogether) {
  for (const auto& [action_id, mojom_action] : kActionMappings) {
    PinAction(action_id, mojom_action);
    EXPECT_NO_FATAL_FAILURE(VerifyPinnedToolbarWidth());
  }

  for (const auto& [action_id, mojom_action] : kActionMappings) {
    UnpinAction(action_id, mojom_action);
  }
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, RouteMediaIcons) {
  auto* action_item = static_cast<actions::StatefulImageActionItem*>(
      actions::ActionManager::Get().FindAction(
          kActionRouteMedia, browser()->GetActions()->root_action_item()));

  const std::vector<std::pair<const gfx::VectorIcon&,
                              toolbar_ui_api::mojom::PinnedToolbarAction>>
      kRouteMediaIcons = {
          {vector_icons::kMediaRouterIdleChromeRefreshIcon,
           toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMediaIdle},
          {vector_icons::kMediaRouterWarningChromeRefreshIcon,
           toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMediaWarning},
          {vector_icons::kMediaRouterPausedIcon,
           toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMediaPaused},
          {vector_icons::kMediaRouterActiveChromeRefreshIcon,
           toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMediaActive},
          {kCastChromeRefreshIcon,
           toolbar_ui_api::mojom::PinnedToolbarAction::kRouteMedia},
      };

  for (const auto& [icon, mojom_action] : kRouteMediaIcons) {
    action_item->SetStatefulImage(ui::ImageModel::FromVectorIcon(icon));
    PinAction(kActionRouteMedia, mojom_action);
    UnpinAction(kActionRouteMedia, mojom_action);
  }
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, SidePanelToggle) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionSidePanelShowCustomizeChrome;
  auto mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kSidePanelShowCustomizeChrome;

  model_->UpdatePinnedState(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
  auto is_any_side_panel_showing = [&]() {
    return side_panel_ui->IsSidePanelShowing();
  };

  // Show side panel.
  EXPECT_TRUE(ClickPinnedButton(web_contents, mojom_action));
  ASSERT_TRUE(base::test::RunUntil(is_any_side_panel_showing));

  // Dismiss side panel.
  EXPECT_TRUE(ClickPinnedButton(web_contents, mojom_action));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !is_any_side_panel_showing(); }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, InvokeActions) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  // QR code generator and translate actions only work with a legitimate
  // non-chrome:// URL
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("/href_translate_test.html")));

  for (const auto& [action_id, mojom_action] : kActionMappings) {
    auto* action_item = actions::ActionManager::Get().FindAction(
        action_id, browser()->GetActions()->root_action_item());
    ASSERT_TRUE(action_item);
    action_item->SetEnabled(true);
    bool invoked = false;
    action_item->SetInvokeActionCallback(base::BindLambdaForTesting(
        [&](actions::ActionItem* item,
            actions::ActionInvocationContext context) { invoked = true; }));

    model_->UpdatePinnedState(action_id, true);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

    EXPECT_TRUE(ClickPinnedButton(web_contents, mojom_action));
    ASSERT_TRUE(base::test::RunUntil([&]() { return invoked; }));
    model_->UpdatePinnedState(action_id, false);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));
  }
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, EphemeralActions) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  // Initially not pinned and not visible.
  ASSERT_FALSE(model_->Contains(action_id));
  ASSERT_FALSE(IsPinnedButtonVisible(web_contents, mojom_action));

  // Show ephemerally.
  webui_toolbar_view->GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(
      action_id, true);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Verify it's highlighted.
  EXPECT_TRUE(EvalJsOnPinnedButton(web_contents, mojom_action,
                                   "return !!btn && "
                                   "btn.hasAttribute('is-menu-open');")
                  .ExtractBool());

  // Hide ephemerally.
  webui_toolbar_view->GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(
      action_id, false);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       UpdateActionState) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  // Initially not pinned and not visible.
  ASSERT_FALSE(model_->Contains(action_id));
  ASSERT_FALSE(IsPinnedButtonVisible(web_contents, mojom_action));

  // Activate action.
  webui_toolbar_view->GetPinnedToolbarActions()->UpdateActionState(action_id,
                                                                   true);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Verify it's highlighted.
  EXPECT_TRUE(EvalJsOnPinnedButton(web_contents, mojom_action,
                                   "return !!btn && "
                                   "btn.hasAttribute('is-menu-open');")
                  .ExtractBool());

  // Deactivate action.
  webui_toolbar_view->GetPinnedToolbarActions()->UpdateActionState(action_id,
                                                                   false);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));

  model_->UpdatePinnedState(action_id, true);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Verify it's not highlighted.
  EXPECT_TRUE(EvalJsOnPinnedButton(web_contents, mojom_action,
                                   "return !!btn && "
                                   "!btn.hasAttribute('is-menu-open');")
                  .ExtractBool());

  // Activate action.
  webui_toolbar_view->GetPinnedToolbarActions()->UpdateActionState(action_id,
                                                                   true);

  // Verify it's highlighted.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsOnPinnedButton(web_contents, mojom_action,
                                "return !!btn && "
                                "btn.hasAttribute('is-menu-open');")
        .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       ButtonEnabledState) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  model_->UpdatePinnedState(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  auto* action_item = actions::ActionManager::Get().FindAction(
      action_id, browser()->GetActions()->root_action_item());
  ASSERT_TRUE(action_item);

  // Disable the action.
  action_item->SetEnabled(false);

  // Verify button is disabled in WebUI.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return EvalJsOnPinnedButton(web_contents, mojom_action,
                                "return !!btn && btn.disabled;")
        .ExtractBool();
  }));

  // Re-enable.
  action_item->SetEnabled(true);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !EvalJsOnPinnedButton(web_contents, mojom_action,
                                 "return !!btn && btn.disabled;")
                .ExtractBool();
  }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, PinUnpinnable) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  model_->UpdatePinnedState(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Make unpinnable.
  SetPinnableProperty(action_id, false);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !IsPinnedButtonVisible(web_contents, mojom_action); }));
  // Make sure it's still pinned.
  ASSERT_TRUE(model_->Contains(action_id));

  // Make pinnable.
  SetPinnableProperty(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, StateAccessors) {
  PinnedToolbarActions* view =
      GetWebUIToolbarWebView(browser())->GetPinnedToolbarActions();

  // Pin then pop out.
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  model_->UpdatePinnedState(kActionPrint, true);
  EXPECT_TRUE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  view->ShowActionEphemerallyInToolbar(kActionPrint, true);
  EXPECT_TRUE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  model_->UpdatePinnedState(kActionPrint, false);
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_TRUE(view->IsActionPoppedOut(kActionPrint));
  view->ShowActionEphemerallyInToolbar(kActionPrint, false);
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));

  // Pop out then pin.
  view->ShowActionEphemerallyInToolbar(kActionPrint, true);
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_TRUE(view->IsActionPoppedOut(kActionPrint));
  model_->UpdatePinnedState(kActionPrint, true);
  EXPECT_TRUE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  view->ShowActionEphemerallyInToolbar(kActionPrint, false);
  EXPECT_TRUE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
  model_->UpdatePinnedState(kActionPrint, false);
  EXPECT_FALSE(view->IsActionPinned(kActionPrint));
  EXPECT_FALSE(view->IsActionPoppedOut(kActionPrint));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       TextAndAriaLabelAttributes) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  actions::ActionId action_id = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  auto* action_item = actions::ActionManager::Get().FindAction(
      action_id, browser()->GetActions()->root_action_item());
  ASSERT_TRUE(action_item);

  // Pin it so it renders.
  model_->UpdatePinnedState(action_id, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return IsPinnedButtonVisible(web_contents, mojom_action); }));

  // Test the default appropriate values are set for the tooltip and ax text.
  std::string default_name =
      base::UTF16ToUTF8(action_item->GetAccessibleName().empty()
                            ? action_item->GetTooltipText()
                            : action_item->GetAccessibleName());
  std::string default_description =
      base::UTF16ToUTF8(action_item->GetTooltipText());

  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         default_name);
  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kButton;
  find_criteria.name = default_name;
  ui::AXPlatformNodeDelegate* print_node =
      content::FindAccessibilityNode(web_contents, find_criteria);
  ASSERT_TRUE(print_node);

  EXPECT_EQ(default_name,
            print_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(default_description, print_node->GetStringAttribute(
                                     ax::mojom::StringAttribute::kDescription));

  // Test all values are provided.
  action_item->SetTooltipText(u"tooltip");
  action_item->SetAccessibleName(u"accessible_name");

  content::WaitForAccessibilityTreeToChange(web_contents);
  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         "accessible_name");
  find_criteria.name = "accessible_name";
  print_node = content::FindAccessibilityNode(web_contents, find_criteria);
  ASSERT_TRUE(print_node);
  EXPECT_EQ("accessible_name",
            print_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("tooltip", print_node->GetStringAttribute(
                           ax::mojom::StringAttribute::kDescription));

  // Test accessible_name is empty (Fallback to Tooltip).
  action_item->SetAccessibleName(u"");

  content::WaitForAccessibilityTreeToChange(web_contents);
  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         "tooltip");
  find_criteria.name = "tooltip";
  print_node = content::FindAccessibilityNode(web_contents, find_criteria);
  ASSERT_TRUE(print_node);
  EXPECT_EQ("tooltip",
            print_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("tooltip", print_node->GetStringAttribute(
                           ax::mojom::StringAttribute::kDescription));

  // Test tooltip and accessible_name are empty.
  action_item->SetTooltipText(u"");

  content::WaitForAccessibilityTreeToChange(web_contents);
  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents, "");
  find_criteria.name = "";
  print_node = content::FindAccessibilityNode(web_contents, find_criteria);
  ASSERT_TRUE(print_node);
  EXPECT_EQ("",
            print_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ("", print_node->GetStringAttribute(
                    ax::mojom::StringAttribute::kDescription));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest, ToolbarDivider) {
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  // Clear any default pinned actions.
  std::vector<actions::ActionId> pinned_ids = model_->PinnedActionIds();
  for (actions::ActionId id : pinned_ids) {
    model_->UpdatePinnedState(id, false);
  }

  auto is_divider_visible = [&]() {
    return content::EvalJs(
               web_contents,
               base::StrCat({GetButtonAppJS("#pinnedToolbarActions"),
                             "?.shadowRoot?.querySelector('toolbar-"
                             "divider') !== null"}))
        .ExtractBool();
  };

  // Helper to check if divider is at expected position.
  // returns index of divider or -1 if not found.
  auto find_divider_index = [&]() {
    return content::EvalJs(web_contents,
                           base::StringPrintf(
                               R"(
      (() => {
        const children = Array.from(%s?.shadowRoot?.children || [])
                            .filter(el => ['pinned-toolbar-action',
                                           'toolbar-divider'].includes(
                                              el.tagName.toLowerCase()));
        return children.findIndex(
            el => el.tagName.toLowerCase() === 'toolbar-divider');
      })();
    )",
                               GetButtonAppJS("#pinnedToolbarActions").c_str()))
        .ExtractInt();
  };

  auto find_action_index =
      [&](toolbar_ui_api::mojom::PinnedToolbarAction action) {
        return content::EvalJs(
                   web_contents,
                   base::StringPrintf(
                       R"(
      (() => {
        const shadowRoot = %s?.shadowRoot;
        if (!shadowRoot) return -1;
        const children = Array.from(shadowRoot.children)
                            .filter(el => ['pinned-toolbar-action',
                                           'toolbar-divider'].includes(
                                              el.tagName.toLowerCase()));
        return children.findIndex(el => el.state && el.state.action === %d);
      })();
    )",
                       GetButtonAppJS("#pinnedToolbarActions").c_str(),
                       static_cast<int>(action)))
            .ExtractInt();
      };

  // 1) Initially no actions, no divider.
  ASSERT_TRUE(base::test::RunUntil([&]() { return !is_divider_visible(); }));

  // 2) Pin one action, divider should appear after it.
  actions::ActionId action1 = kActionPrint;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action1 =
      toolbar_ui_api::mojom::PinnedToolbarAction::kPrint;

  model_->UpdatePinnedState(action1, true);
  ASSERT_TRUE(base::test::RunUntil([&]() { return is_divider_visible(); }));

  int action1_index = find_action_index(mojom_action1);
  int divider_index = find_divider_index();
  EXPECT_EQ(divider_index, action1_index + 1);

  // 3) Pop out another action, divider should be between them.
  actions::ActionId action2 = kActionShowTranslate;
  toolbar_ui_api::mojom::PinnedToolbarAction mojom_action2 =
      toolbar_ui_api::mojom::PinnedToolbarAction::kShowTranslate;

  webui_toolbar_view->GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(
      action2, true);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return find_action_index(mojom_action2) != -1; }));

  action1_index = find_action_index(mojom_action1);
  divider_index = find_divider_index();
  int action2_index = find_action_index(mojom_action2);

  EXPECT_LT(action1_index, divider_index);
  EXPECT_LT(divider_index, action2_index);
  EXPECT_EQ(divider_index, action1_index + 1);
  EXPECT_EQ(action2_index, divider_index + 1);

  // 4) Unpin action1 and hide action2 ephemerally, divider should disappear.
  model_->UpdatePinnedState(action1, false);
  webui_toolbar_view->GetPinnedToolbarActions()->ShowActionEphemerallyInToolbar(
      action2, false);
  ASSERT_TRUE(base::test::RunUntil([&]() { return !is_divider_visible(); }));
}

IN_PROC_BROWSER_TEST_F(WebUIPinnedToolbarActionsBrowserTest,
                       A11yAnnouncements) {
  AXAnnouncementObserver announcement_observer(views::AXUpdateNotifier::Get());

  actions::ActionId action_id = kActionSendSharedTabGroupFeedback;

  // Initial State: Unpinned.
  ASSERT_FALSE(model_->Contains(action_id));

  auto invoke_pin_unpin = [&](actions::ActionId pin_unpin_action) {
    actions::ActionManager::Get()
        .FindAction(pin_unpin_action,
                    browser()->GetActions()->root_action_item())
        ->InvokeAction(actions::ActionInvocationContext::Builder()
                           .SetProperty(kActionIdKey, action_id)
                           .Build());
  };

  // Pin via Action Invocation.
  invoke_pin_unpin(kActionPinActionToToolbar);

  // Expect an announcement that the action was pinned.
  EXPECT_TRUE(announcement_observer.verify_last_announcement(
      IDS_TOOLBAR_BUTTON_PINNED));
  ASSERT_TRUE(model_->Contains(action_id));

  // Unpin via Action Invocation.
  invoke_pin_unpin(kActionUnpinActionFromToolbar);

  // Expect an announcement that the action was unpinned.
  EXPECT_TRUE(announcement_observer.verify_last_announcement(
      IDS_TOOLBAR_BUTTON_UNPINNED));
  ASSERT_FALSE(model_->Contains(action_id));
}

struct DragTestParam {
  const char* test_name;
  const char* selector;
  const char* pref_name = nullptr;
};

class WebUIToolbarButtonPressAndDragTest
    : public WebUIToolbarWebViewBrowserTest,
      public testing::WithParamInterface<DragTestParam> {
 public:
  WebUIToolbarButtonPressAndDragTest()
      : WebUIToolbarWebViewBrowserTest(
            {features::kInitialWebUI, features::kWebUIReloadButton,
             features::kWebUISplitTabsButton, features::kWebUIHomeButton,
             features::kWebUIBackForwardButton,
             features::kSkipIPCChannelPausingForNonGuests,
             features::kWebUIInProcessResourceLoadingV2,
             features::kInitialWebUISyncNavStartToCommit},
            {}) {}
};

IN_PROC_BROWSER_TEST_P(WebUIToolbarButtonPressAndDragTest, PressAndDragDown) {
  const auto& param = GetParam();
  WebUIToolbarWebView* webui_toolbar_view = GetWebUIToolbarWebView(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();
  content::WebContents* web_contents = web_view->GetWebContents();

  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  content::WaitForCopyableViewInWebContents(web_contents);

  if (param.pref_name) {
    PinButton(browser(), web_view, param.pref_name);
  }

  if (std::string(param.test_name) == "Reload") {
    webui_toolbar_view->reload_control_.SetDevToolsStatus(true);
  }

  if (std::string(param.test_name) == "Back" ||
      std::string(param.test_name) == "Forward") {
    // Navigate twice to ensure we have back/forward history.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab")));
    if (std::string(param.test_name) == "Forward") {
      chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
    }
  }

  ASSERT_TRUE(WaitForButtonVisible(web_contents, param.selector));

  // Inject mocks for pointer capture to avoid errors with synthetic events.
  std::ignore = content::ExecJs(
      web_contents,
      "HTMLElement.prototype.setPointerCapture = () => {}; "
      "HTMLElement.prototype.releasePointerCapture = () => {};");

  // Wait for the inner icon button.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return content::EvalJs(
               web_contents,
               base::StrCat({GetButtonIconJS(param.selector), " !== null"}))
        .ExtractBool();
  }));

  // Identify the control to check for menu_runner.
  auto get_menu_runner = [&]() -> views::MenuRunner* {
    if (std::string(param.selector) == kReloadButtonSelector) {
      return webui_toolbar_view->reload_control_.menu_runner_.get();
    } else if (std::string(param.selector) == kHomeSelector) {
      return webui_toolbar_view->home_control_.menu_runner_.get();
    } else if (std::string(param.selector) == kBackSelector) {
      return webui_toolbar_view->back_control_.menu_runner_.get();
    } else if (std::string(param.selector) == kForwardSelector) {
      return webui_toolbar_view->forward_control_.menu_runner_.get();
    }
    return nullptr;
  };

  views::MenuRunner* initial_runner = get_menu_runner();
  EXPECT_TRUE(!initial_runner || !initial_runner->IsRunning());

  // Start with pointerdown.
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      DispatchPointerEvent("pointerdown", param.selector, "mouse")));

  // Simulate downward drag by 10px.
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      base::StringPrintf(
          "(() => { "
          "  HTMLElement.prototype.setPointerCapture = () => {}; "
          "  HTMLElement.prototype.releasePointerCapture = () => {}; "
          "  const target = %s; "
          "  if (target) { "
          "    const rect = target.getBoundingClientRect(); "
          "    const x = rect.left + rect.width / 2; "
          "    const y = rect.top + rect.height / 2; "
          "    target.dispatchEvent(new PointerEvent('pointermove', "
          "    {bubbles: true, cancelable: true, view: window, pointerType: "
          "    'mouse', "
          "    clientX: x, clientY: y + 10})); "
          "  } "
          "})();",
          GetButtonIconJS(param.selector).c_str())));

  // The menu should open immediately.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    views::MenuRunner* runner = get_menu_runner();
    return runner && runner->IsRunning();
  }));

  // Clean up
  get_menu_runner()->Cancel();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebUIToolbarButtonPressAndDragTest,
    testing::Values(
        DragTestParam{.test_name = "Reload", .selector = kReloadButtonSelector},
        DragTestParam{.test_name = "Home",
                      .selector = kHomeSelector,
                      .pref_name = prefs::kShowHomeButton},
        DragTestParam{.test_name = "Back", .selector = kBackSelector},
        DragTestParam{.test_name = "Forward",
                      .selector = kForwardSelector,
                      .pref_name = prefs::kShowForwardButton}),
    [](const testing::TestParamInfo<DragTestParam>& info) {
      return info.param.test_name;
    });

class WebUIToolbarProcessOverheadExperimentBrowserTest
    : public InProcessBrowserTest {
 public:
  WebUIToolbarProcessOverheadExperimentBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kWebUIToolbarProcessOverheadExperiment},
        {features::kWebUIReloadButton});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarProcessOverheadExperimentBrowserTest,
                       Basic) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ToolbarView* toolbar_view = browser_view->toolbar();

  // Verify that the C++ reload button is visible.
  views::View* reload_button = toolbar_view->reload_button();
  ASSERT_TRUE(reload_button);
  EXPECT_TRUE(reload_button->GetVisible());

  // Verify that the WebUIToolbarWebView is NOT in the view hierarchy.
  ToolbarButtonProvider* provider = toolbar_view;
  EXPECT_EQ(provider->GetWebUIToolbarViewForTesting(), nullptr);

  // Verify that the detached WebUIToolbarWebView IS created.
  EXPECT_NE(toolbar_view->detached_toolbar_webview_for_testing(), nullptr);
}

class WebUIToolbarAlreadyExistsForTheSameProfileOnInitTest
    : public WebUIToolbarWebViewBrowserTest {
 public:
  WebUIToolbarAlreadyExistsForTheSameProfileOnInitTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    WebUIToolbarWebViewBrowserTest::SetUpInProcessBrowserTestFixture();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(WebUIToolbarAlreadyExistsForTheSameProfileOnInitTest,
                       FirstProcessRecordsFalse) {
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester_->ExpectUniqueSample(
      "InitialWebUI.Toolbar.ProcessAlreadyExistsForTheSameProfileOnCreation",
      false, 1);
}
