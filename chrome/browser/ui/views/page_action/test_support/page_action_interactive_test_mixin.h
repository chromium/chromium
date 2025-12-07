// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/state_observer.h"

DECLARE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                               kPageActionButtonVisible);

// Template usable as a mixin class for any Page Action tests extending
// InteractiveBrowserTestApi.
template <typename T>
  requires(std::derived_from<T, InteractiveBrowserTestApi>)
class PageActionInteractiveTestMixin : public T {
 public:
  template <class... Args>

  explicit PageActionInteractiveTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

 protected:
  // Utility to reliably wait for the page action view to be visible.
  auto WaitForPageActionButtonVisible(actions::ActionId action_id) {
    IconLabelBubbleView* page_action =
        BrowserView::GetBrowserViewForBrowser(T::browser())
            ->toolbar_button_provider()
            ->GetPageActionView(action_id);

    constexpr char kPageActionId[] = "page action view";
    return T::Steps(T::NameView(kPageActionId, page_action),
                    T::WaitForShow(kPageActionId));
  }

  // Utility to invoke the page action.
  auto InvokePageAction(actions::ActionId action_id) {
    IconLabelBubbleView* page_action =
        BrowserView::GetBrowserViewForBrowser(T::browser())
            ->toolbar_button_provider()
            ->GetPageActionView(action_id);

    constexpr char kPageActionId[] = "page action view";
    return T::Steps(T::NameView(kPageActionId, page_action),
                    T::PressButton(kPageActionId));
  }

  // Utility to reliably wait for the page action view to be visible in chip
  // state.
  auto WaitForPageActionChipVisible(actions::ActionId action_id) {
    auto steps = T::Steps(
        T::PollState(kPageActionButtonVisible,
                     [this, action_id]() {
                       auto* view =
                           BrowserView::GetBrowserViewForBrowser(T::browser())
                               ->toolbar_button_provider()
                               ->GetPageActionView(action_id);
                       return view->GetVisible() && !view->is_animating_label();
                     }),
        T::WaitForState(kPageActionButtonVisible, true),
        T::StopObservingState(kPageActionButtonVisible));
    T::AddDescriptionPrefix(steps, "WaitForPageActionChipVisible()");
    return steps;
  }

  // Utility to reliably wait for the page action view to not be visible in chip
  // state.
  auto WaitForPageActionChipNotVisible(actions::ActionId action_id) {
    auto steps = T::Steps(
        T::PollState(kPageActionButtonVisible,
                     [this, action_id]() {
                       auto* view =
                           BrowserView::GetBrowserViewForBrowser(T::browser())
                               ->toolbar_button_provider()
                               ->GetPageActionView(action_id);
                       return view->GetVisible() && !view->is_animating_label();
                     }),
        T::WaitForState(kPageActionButtonVisible, false),
        T::StopObservingState(kPageActionButtonVisible));
    T::AddDescriptionPrefix(steps, "WaitForPageActionChipNotVisible()");
    return steps;
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_INTERACTIVE_TEST_MIXIN_H_
