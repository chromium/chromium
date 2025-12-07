// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/update_user_activation_state_interceptor.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

namespace {

// Facilitates pixel testing with more versatile naming than TestBrowserUi.
class PixelTestUi : public TestBrowserUi {
 public:
  PixelTestUi(views::View* view,
              const std::string& screenshot_name,
              const std::string& baseline,
              const ScreenshotOptions& options)
      : view_(view),
        screenshot_name_(screenshot_name),
        baseline_(baseline),
        options_(options) {}
  ~PixelTestUi() override = default;

  // TestBrowserUi:
  void ShowUi(const std::string& name) override { NOTREACHED(); }
  void WaitForUserDismissal() override { NOTREACHED(); }

  bool VerifyUi() override {
    return VerifyUiWithResult() != ui::test::ActionResult::kFailed;
  }

  ui::test::ActionResult VerifyUiWithResult() {
    auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string test_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});
    const std::string screenshot_name =
        screenshot_name_.empty()
            ? baseline_
            : base::StrCat({screenshot_name_, "_", baseline_});
    return VerifyPixelUi(view_, options_, test_name, screenshot_name);
  }

 private:
  raw_ptr<views::View> view_ = nullptr;
  std::string screenshot_name_;
  std::string baseline_;
  ScreenshotOptions options_;
};

views::View* GetScreenshotTargetView(ui::TrackedElement* element) {
  if (auto* const view_el = element->AsA<views::TrackedElementViews>()) {
    return view_el->view();
  } else if (auto* const page_el = element->AsA<TrackedElementWebContents>()) {
    return page_el->owner()->GetWebView();
  }
  return nullptr;
}

ui::test::ActionResult CompareScreenshotCommon(
    views::View* view,
    const ScreenshotOptions& options,
    const std::string& screenshot_name,
    const std::string& baseline_cl) {
  // pixel_browser_tests and pixel_interactive_ui_tests specify this command
  // line, which is checked by TestBrowserUi before attempting any screen
  // capture; otherwise screenshotting is a silent no-op.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVerifyPixels)) {
    LOG(WARNING)
        << "Cannot take screenshot: pixel test command line not set. This is "
           "normal for non-pixel-test jobs such as vanilla browser_tests.";
    return ui::test::ActionResult::kKnownIncompatible;
  }

  PixelTestUi pixel_test_ui(view, screenshot_name, baseline_cl, options);
  ui::test::ActionResult result = pixel_test_ui.VerifyUiWithResult();
  if (result == ui::test::ActionResult::kKnownIncompatible) {
    LOG(WARNING) << "Current platform does not support pixel tests.";
  }
  return result;
}

// Special handler for browsers and browser tab strips that enables SelectTab().
class InteractionTestUtilSimulatorBrowser
    : public ui::test::InteractionTestUtil::Simulator {
 public:
  InteractionTestUtilSimulatorBrowser() = default;
  ~InteractionTestUtilSimulatorBrowser() override = default;

  ui::test::ActionResult SendAccelerator(ui::TrackedElement* element,
                                         ui::Accelerator accelerator) override {
#if BUILDFLAG(IS_MAC)
    // Browser accelerators must be sent via key events to the window on Mac or
    // they don't work properly. Dialog accelerators still appear to work the
    // same as on other platforms.
    if (auto* const bwi = InteractionTestUtilBrowser::GetBrowserFromContext(
            element->context())) {
      if (!ui_controls::SendKeyPress(
              BrowserView::GetBrowserViewForBrowser(bwi)->GetNativeWindow(),
              accelerator.key_code(), accelerator.IsCtrlDown(),
              accelerator.IsShiftDown(), accelerator.IsAltDown(),
              accelerator.IsCmdDown())) {
        LOG(ERROR) << "Failed to send accelerator"
                   << accelerator.GetShortcutText() << " to " << *element;
        return ui::test::ActionResult::kFailed;
      }
      return ui::test::ActionResult::kSucceeded;
    }
#endif  // BUILDFLAG(IS_MAC)

    if (auto* const tracked_contents =
            element->AsA<TrackedElementWebContents>()) {
      if (auto* const view = tracked_contents->owner()->GetWebView()) {
        // There are two possibilities:
        //  1. This is a legitimate accelerator, which must be handled by the
        //     focus manager.
        //  2. This is a control key that will be processed by Blink (e.g.
        //     pressing enter or space to "click" an HTML button), and therefore
        //     must be injected as a keypress.
        bool result = view->GetFocusManager()->ProcessAccelerator(accelerator);
        if (!result) {
          result = ui_controls::SendKeyPress(
              tracked_contents->owner()
                  ->web_contents()
                  ->GetTopLevelNativeWindow(),
              accelerator.key_code(), accelerator.IsCtrlDown(),
              accelerator.IsShiftDown(), accelerator.IsAltDown(),
              accelerator.IsCmdDown());
        }
        return result ? ui::test::ActionResult::kSucceeded
                      : ui::test::ActionResult::kFailed;
      } else {
        LOG(ERROR) << "No associated view to send accelerators to.";
        return ui::test::ActionResult::kFailed;
      }
    }

    return ui::test::ActionResult::kNotAttempted;
  }

  // Handle sending key presses to web contents. All other cases are handled by
  // the Views simulator.
  ui::test::ActionResult SendKeyPress(ui::TrackedElement* element,
                                      ui::KeyboardCode key,
                                      int flags) override {
    if (auto* const tracked_contents =
            element->AsA<TrackedElementWebContents>()) {
      auto native_window =
          tracked_contents->owner()->web_contents()->GetTopLevelNativeWindow();
      const bool result = ui_test_utils::SendKeyPressToWindowSync(
          native_window, key, flags & ui::EF_CONTROL_DOWN,
          flags & ui::EF_SHIFT_DOWN, flags & ui::EF_ALT_DOWN,
          flags & ui::EF_COMMAND_DOWN);
      return result ? ui::test::ActionResult::kSucceeded
                    : ui::test::ActionResult::kFailed;
    }

    return ui::test::ActionResult::kNotAttempted;
  }

  // Chrome has better and more thorough functionality for bringing a browser
  // window to the front, but it's expensive, so only actually use it for
  // browser windows on platforms where activation requires extra steps.
  ui::test::ActionResult ActivateSurface(ui::TrackedElement* el) override {
    views::View* view = nullptr;
    bool is_web_contents = false;
    if (auto* view_el = el->AsA<views::TrackedElementViews>()) {
      view = view_el->view();
    } else if (auto* contents_el = el->AsA<TrackedElementWebContents>()) {
      is_web_contents = true;
      view = contents_el->owner()->GetWebView();
      if (!view) {
        LOG(ERROR) << "WebContents not associated with any UI element.";
        return ui::test::ActionResult::kFailed;
      }
    }
    if (!view) {
      return ui::test::ActionResult::kNotAttempted;
    }

    // Get the browser and browser window associated with the current context.
    // If there is none, or it is not the same widget as the view, do not use
    // this implementation.
    if (auto* const browser =
            InteractionTestUtilBrowser::GetBrowserFromContext(el->context())) {
      if (auto* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser)) {
        if (browser_view->GetWidget() == view->GetWidget()) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
          // Bring the browser window to the front using the most aggressive
          // method for the current platform. If this is not done, then mouse
          // events might not get routed to the correct surface.
          if (!ui_test_utils::BringBrowserWindowToFront(browser)) {
            LOG(ERROR) << "BringBrowserWindowToFront() failed.";
            return ui::test::ActionResult::kFailed;
          }
          return ui::test::ActionResult::kSucceeded;
#else
          // Use the default logic to activate the browser.
          return views::test::InteractionTestUtilSimulatorViews::ActivateWidget(
              view->GetWidget());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
        }
      }
    }

    // Because the fallback Views handler doesn't know about WebContents, handle
    // activation here.
    if (is_web_contents) {
      return views::test::InteractionTestUtilSimulatorViews::ActivateWidget(
          view->GetWidget());
    }

    return ui::test::ActionResult::kNotAttempted;
  }

  ui::test::ActionResult FocusElement(ui::TrackedElement* element) override {
    auto* const instrumented = element->AsA<TrackedElementWebContents>();
    if (!instrumented) {
      return ui::test::ActionResult::kNotAttempted;
    }

    auto* const contents = instrumented->owner()->web_contents();
    if (!contents) {
      LOG(ERROR) << "WebContents not present.";
      return ui::test::ActionResult::kFailed;
    }

    // Focus the renderer.
    if (!contents->GetRenderWidgetHostView()) {
      LOG(ERROR) << "No render widget host.";
      return ui::test::ActionResult::kFailed;
    }
    contents->GetRenderWidgetHostView()->Focus();

    // Prepare the renderer for input.
    if (!contents->GetPrimaryMainFrame()) {
      LOG(ERROR) << "No main frame.";
      return ui::test::ActionResult::kFailed;
    }
    content::UpdateUserActivationStateInterceptor user_activation_interceptor(
        contents->GetPrimaryMainFrame());
    user_activation_interceptor.UpdateUserActivationState(
        blink::mojom::UserActivationUpdateType::kNotifyActivation,
        blink::mojom::UserActivationNotificationType::kTest);

    // Ensure the correct WebView is focused before returning.
    auto* web_view = instrumented->owner()->GetWebView();
    if (!web_view) {
      LOG(ERROR) << "No web view.";
      return ui::test::ActionResult::kFailed;
    }
    views::test::ViewFocusedWaiter focus_waiter(*web_view);
    focus_waiter.Wait();

    return ui::test::ActionResult::kSucceeded;
  }

  ui::test::ActionResult SelectTab(
      ui::TrackedElement* tab_collection,
      size_t index,
      InputType input_type,
      std::optional<size_t> expected_index_after_selection) override {
    // This handler *explicitly* only handles browser and TabStrip; it will
    // reject any other element or View type.
    if (!tab_collection->IsA<views::TrackedElementViews>())
      return ui::test::ActionResult::kNotAttempted;
    auto* const view =
        tab_collection->AsA<views::TrackedElementViews>()->view();
    TabStrip* tab_strip = nullptr;
    if (auto* const browser_view = views::AsViewClass<BrowserView>(view)) {
      tab_strip = browser_view->tabstrip();
    } else {
      tab_strip = views::AsViewClass<TabStrip>(view);
    }
    if (!tab_strip)
      return ui::test::ActionResult::kNotAttempted;

    // Verify that the tab index is in range; at this point it's a fatal error
    // if it's out of bounds.
    if (static_cast<int>(index) >= tab_strip->GetTabCount()) {
      LOG(ERROR) << "Tabstrip index " << index
                 << " is out of bounds, there are " << tab_strip->GetTabCount()
                 << " tabs.";
      return ui::test::ActionResult::kFailed;
    }

    // Tabs can be selected using a default action; no special input logic is
    // needed.
    Tab* const tab = tab_strip->tab_at(index);
    views::test::InteractionTestUtilSimulatorViews::DoDefaultAction(tab,
                                                                    input_type);

    const int expected =
        static_cast<int>(expected_index_after_selection.value_or(index));
    if (expected != tab_strip->GetActiveIndex()) {
      LOG(ERROR) << "Failed to select tabstrip tab " << index;
      return ui::test::ActionResult::kFailed;
    }
    return ui::test::ActionResult::kSucceeded;
  }

  ui::test::ActionResult Confirm(ui::TrackedElement* element) override {
    // This handler *explicitly* only handles OmniboxView; it will reject any
    // other element or View type.
    if (!element->IsA<views::TrackedElementViews>())
      return ui::test::ActionResult::kNotAttempted;
    auto* const view = element->AsA<views::TrackedElementViews>()->view();
    if (auto* const omnibox = views::AsViewClass<OmniboxViewViews>(view)) {
      ui::KeyEvent press(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                         ui::EF_NONE);
      omnibox->OnKeyEvent(&press);
      ui::KeyEvent release(ui::EventType::kKeyReleased, ui::VKEY_RETURN,
                           ui::EF_NONE);
      omnibox->OnKeyEvent(&release);
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kNotAttempted;
  }
};

}  // namespace

// static
void InteractionTestUtilBrowser::PopulateSimulators(
    ui::test::InteractionTestUtil& test_util) {
  test_util.AddSimulator(
      std::make_unique<InteractionTestUtilSimulatorBrowser>());
}

// static
BrowserWindowInterface* InteractionTestUtilBrowser::GetBrowserFromContext(
    ui::ElementContext context) {
  BrowserWindowInterface* result = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (auto* elements = BrowserElements::From(browser)) {
          if (elements->GetContext() == context) {
            result = browser;
          }
        }
        return !result;
      });
  return result;
}

// static
ui::test::ActionResult InteractionTestUtilBrowser::CompareScreenshot(
    ui::TrackedElement* element,
    const std::string& screenshot_name,
    const std::string& baseline_cl,
    const ScreenshotOptions& options) {
  views::View* const view = GetScreenshotTargetView(element);
  if (!view) {
    return ui::test::ActionResult::kNotAttempted;
  }
  return CompareScreenshotCommon(view, options, screenshot_name, baseline_cl);
}

// static
ui::test::ActionResult InteractionTestUtilBrowser::CompareSurfaceScreenshot(
    ui::TrackedElement* element_in_surface,
    const std::string& screenshot_name,
    const std::string& baseline_cl) {
  views::View* const view = GetScreenshotTargetView(element_in_surface);
  if (!view || !view->GetWidget()) {
    return ui::test::ActionResult::kNotAttempted;
  }
  return CompareScreenshotCommon(view->GetWidget()->GetRootView(), {},
                                 screenshot_name, baseline_cl);
}
