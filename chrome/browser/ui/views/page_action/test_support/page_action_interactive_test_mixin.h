// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_support.h"
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
    page_actions::PageActionPropertiesProvider provider;
    CHECK(provider.Contains(action_id));
    ui::ElementIdentifier element_id =
        provider.GetProperties(action_id).element_identifier;
    if (element_id) {
      return T::Steps(T::WaitForShow(element_id));
    }
    return WaitForPageActionState(
        action_id,
        base::BindRepeating(&page_actions::PageActionTestAccessor::GetVisible),
        "WaitForPageActionButtonVisible()");
  }

  // Utility to invoke the page action.
  auto InvokePageAction(actions::ActionId action_id) {
    page_actions::PageActionPropertiesProvider provider;
    CHECK(provider.Contains(action_id));
    ui::ElementIdentifier element_id =
        provider.GetProperties(action_id).element_identifier;
    if (element_id) {
      return T::Steps(T::PressButton(element_id));
    }
    return T::Steps(T::Do([this, action_id]() {
      page_actions::PageActionTestAccessor(T::browser(), action_id).Click();
    }));
  }

  // Utility to reliably wait for the page action view to be visible in chip
  // state.
  auto WaitForPageActionChipVisible(actions::ActionId action_id) {
    return WaitForPageActionState(
        action_id,
        base::BindRepeating(
            [](const page_actions::PageActionTestAccessor* accessor) {
              return accessor->IsChipVisible() && !accessor->IsAnimating();
            }),
        "WaitForPageActionChipVisible()");
  }

  // Utility to reliably wait for the page action view to be visible in icon
  // state.
  auto WaitForPageActionIconVisible(actions::ActionId action_id) {
    return WaitForPageActionState(
        action_id,
        base::BindRepeating(
            &page_actions::PageActionTestAccessor::IsIconVisible),
        "WaitForPageActionIconVisible()");
  }

  // Utility to reliably wait for the page action view to not be visible in chip
  // state.
  auto WaitForPageActionChipNotVisible(actions::ActionId action_id) {
    return WaitForPageActionState(
        action_id,
        base::BindRepeating(
            [](const page_actions::PageActionTestAccessor* accessor) {
              return !accessor->IsChipVisible() && !accessor->IsAnimating();
            }),
        "WaitForPageActionChipNotVisible()");
  }

 private:
  auto WaitForPageActionState(
      actions::ActionId action_id,
      base::RepeatingCallback<bool(const page_actions::PageActionTestAccessor*)>
          matcher,
      std::string_view description) {
    auto steps =
        T::Steps(T::PollState(kPageActionButtonVisible,
                              [this, action_id, matcher]() {
                                page_actions::PageActionTestAccessor accessor(
                                    T::browser(), action_id);
                                return matcher.Run(&accessor);
                              }),
                 T::WaitForState(kPageActionButtonVisible, true),
                 T::StopObservingState(kPageActionButtonVisible));
    T::AddDescriptionPrefix(steps, description);
    return steps;
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_INTERACTIVE_TEST_MIXIN_H_
