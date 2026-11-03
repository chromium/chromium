// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/test_support/media_toolbar_button_test_accessor.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/tracked_element/interaction_test_util_web_ui.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

MediaToolbarButtonTestAccessor::MediaToolbarButtonTestAccessor(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

MediaToolbarButtonTestAccessor::~MediaToolbarButtonTestAccessor() = default;

MediaToolbarButton* MediaToolbarButtonTestAccessor::GetMediaButton() const {
  if (!browser_) {
    return nullptr;
  }
  LayoutBrowserIfNecessary();
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (!browser_view || !browser_view->toolbar()) {
    return nullptr;
  }
  return browser_view->toolbar()->media_button();
}

bool MediaToolbarButtonTestAccessor::IsVisible() const {
  MediaToolbarButton* const media_button = GetMediaButton();
  return media_button && media_button->IsVisible();
}

ui::TrackedElement* MediaToolbarButtonTestAccessor::GetElement() const {
  if (!browser_) {
    return nullptr;
  }
  LayoutBrowserIfNecessary();
  if (auto* const browser_elements = BrowserElements::From(browser_)) {
    return browser_elements->GetElement(kToolbarMediaButtonElementId);
  }
  return nullptr;
}

void MediaToolbarButtonTestAccessor::Click() const {
  ui::TrackedElement* element = GetElement();
  // In WebUI, `IsVisible()` becomes true synchronously when the controller
  // shows the button, before the renderer registers the TrackedElement.
  if (!element && IsVisible()) {
    if (auto* const browser_elements = BrowserElements::From(browser_)) {
      base::RunLoop run_loop;
      auto subscription =
          ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
              kToolbarMediaButtonElementId, browser_elements->GetContext(),
              base::IgnoreArgs<ui::TrackedElement*>(run_loop.QuitClosure()));
      run_loop.Run();
      element = GetElement();
    }
  }
  ASSERT_TRUE(element);
  const bool is_webui = element->IsA<ui::TrackedElementWebUI>();
  ui::test::InteractionTestUtil test_util;
  test_util.AddSimulator(
      std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
  test_util.AddSimulator(
      std::make_unique<ui::InteractionTestUtilSimulatorWebUI>());
  // Use `kKeyboard` (detail === 0) for WebUI to bypass `HighlightTracker`
  // pointerdown click suppression when toggling an already-open dialog in
  // tests where the bubble widget may not deactivate on synthetic pointerdown.
  const auto input_type =
      is_webui ? ui::test::InteractionTestUtil::InputType::kKeyboard
               : ui::test::InteractionTestUtil::InputType::kMouse;
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.PressButton(element, input_type));
  if (is_webui) {
    EXPECT_TRUE(FlushWebUIToolbarPipes(browser_));
  }
}

void MediaToolbarButtonTestAccessor::LayoutBrowserIfNecessary() const {
  if (!browser_) {
    return;
  }
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (browser_view && browser_view->GetWidget()) {
    browser_view->GetWidget()->LayoutRootViewIfNecessary();
  }
}
