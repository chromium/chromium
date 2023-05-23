// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event.h"
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
              const std::string& baseline)
      : view_(view), screenshot_name_(screenshot_name), baseline_(baseline) {}
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
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});
    const std::string screenshot_name =
        screenshot_name_.empty()
            ? baseline_
            : base::StrCat({screenshot_name_, "_", baseline_});
    return VerifyPixelUi(view_, test_name, screenshot_name);
  }

 private:
  raw_ptr<views::View> view_ = nullptr;
  std::string screenshot_name_;
  std::string baseline_;
};

// Special handler for browsers and browser tab strips that enables SelectTab().
class InteractionTestUtilSimulatorBrowser
    : public ui::test::InteractionTestUtil::Simulator {
 public:
  InteractionTestUtilSimulatorBrowser() = default;
  ~InteractionTestUtilSimulatorBrowser() override = default;

#if BUILDFLAG(IS_MAC)
  // Browser accelerators must be sent via key events to the window on Mac or
  // they don't work properly. Dialog accelerators still appear to work the same
  // as on other platforms.
  ui::test::ActionResult SendAccelerator(ui::TrackedElement* element,
                                         ui::Accelerator accelerator) override {
    Browser* const browser =
        InteractionTestUtilBrowser::GetBrowserFromContext(element->context());
    if (!browser)
      return ui::test::ActionResult::kNotAttempted;

    if (!ui_controls::SendKeyPress(
            browser->window()->GetNativeWindow(), accelerator.key_code(),
            accelerator.IsCtrlDown(), accelerator.IsShiftDown(),
            accelerator.IsAltDown(), accelerator.IsCmdDown())) {
      LOG(ERROR) << "Failed to send accelerator"
                 << accelerator.GetShortcutText() << " to " << *element;
      return ui::test::ActionResult::kFailed;
    }
    return ui::test::ActionResult::kSucceeded;
  }
#endif  // BUILDFLAG(IS_MAC)

  // Chrome has better and more thorough functionality for bringing a browser
  // window to the front, but it's expensive, so only actually use it for
  // browser windows on platforms where activation requires extra steps.
  ui::test::ActionResult ActivateSurface(ui::TrackedElement* el) override {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    if (!el->IsA<views::TrackedElementViews>()) {
      return ui::test::ActionResult::kNotAttempted;
    }

    // Get the browser and browser window associated with the current context.
    // If there is none, do not use this implementation.
    auto* const browser =
        InteractionTestUtilBrowser::GetBrowserFromContext(el->context());
    if (!browser) {
      return ui::test::ActionResult::kNotAttempted;
    }
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (!browser_view) {
      return ui::test::ActionResult::kNotAttempted;
    }

    // If the target widget is not the primary window widget, do not use this
    // implementation.
    if (browser_view->GetWidget() !=
        el->AsA<views::TrackedElementViews>()->view()->GetWidget()) {
      return ui::test::ActionResult::kNotAttempted;
    }

    // Bring the browser window to the front using the most aggressive method
    // for the current platform. If this is not done, then mouse events might
    // not get routed to the correct surface.
    if (!ui_test_utils::BringBrowserWindowToFront(browser)) {
      LOG(ERROR) << "BringBrowserWindowToFront() failed.";
      return ui::test::ActionResult::kFailed;
    }
    return ui::test::ActionResult::kSucceeded;
#else
    return ui::test::ActionResult::kNotAttempted;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  }

  ui::test::ActionResult SelectTab(ui::TrackedElement* tab_collection,
                                   size_t index,
                                   InputType input_type) override {
    // This handler *explicitly* only handles Browser and TabStrip; it will
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
    if (static_cast<int>(index) != tab_strip->GetActiveIndex()) {
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
      ui::KeyEvent press(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, ui::EF_NONE);
      omnibox->OnKeyEvent(&press);
      ui::KeyEvent release(ui::ET_KEY_RELEASED, ui::VKEY_RETURN, ui::EF_NONE);
      omnibox->OnKeyEvent(&release);
      return ui::test::ActionResult::kSucceeded;
    }
    return ui::test::ActionResult::kNotAttempted;
  }
};

}  // namespace

InteractionTestUtilBrowser::InteractionTestUtilBrowser() {
  AddSimulator(std::make_unique<InteractionTestUtilSimulatorBrowser>());
  AddSimulator(
      std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
#if BUILDFLAG(IS_MAC)
  AddSimulator(std::make_unique<ui::test::InteractionTestUtilSimulatorMac>());
#endif
}

InteractionTestUtilBrowser::~InteractionTestUtilBrowser() = default;

// static
Browser* InteractionTestUtilBrowser::GetBrowserFromContext(
    ui::ElementContext context) {
  BrowserList* const browsers = BrowserList::GetInstance();
  for (Browser* const browser : *browsers) {
    if (browser->window()->GetElementContext() == context)
      return browser;
  }
  return nullptr;
}

// static
ui::test::ActionResult InteractionTestUtilBrowser::CompareScreenshot(
    ui::TrackedElement* element,
    const std::string& screenshot_name,
    const std::string& baseline) {
  views::View* view = nullptr;
  if (auto* const view_el = element->AsA<views::TrackedElementViews>()) {
    view = view_el->view();
  } else if (auto* const page_el = element->AsA<TrackedElementWebContents>()) {
    view = page_el->owner()->GetWebView();
  }
  if (!view) {
    return ui::test::ActionResult::kNotAttempted;
  }

  // pixel_browser_tests and pixel_interactive_ui_tests specify this command
  // line, which is checked by TestBrowserUi before attempting any screen
  // capture; otherwise screenshotting is a silent no-op.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          "browser-ui-tests-verify-pixels")) {
    LOG(WARNING)
        << "Cannot take screenshot: pixel test command line not set. This is "
           "normal for non-pixel-test jobs such as vanilla browser_tests.";
    return ui::test::ActionResult::kKnownIncompatible;
  }

  PixelTestUi pixel_test_ui(view, screenshot_name, baseline);
  ui::test::ActionResult result = pixel_test_ui.VerifyUiWithResult();
  if (result == ui::test::ActionResult::kKnownIncompatible) {
    LOG(WARNING) << "Current platform does not support pixel tests.";
  }
  return result;
}
