// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TEST_SUPPORT_WEBUI_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_WEBUI_TEST_SUPPORT_WEBUI_INTERACTIVE_TEST_MIXIN_H_

#include <type_traits>
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "ui/base/interaction/interactive_test.h"

namespace {
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementRenders);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonWasClicked);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kIronCollapseContentShows);
}  // namespace

// Template to be used as a mixin class for performance settings webui
// interactive tests
template <typename T>
class WebUiInteractiveTestMixin : public T {
 public:
  template <class... Args>
  explicit WebUiInteractiveTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~WebUiInteractiveTestMixin() override = default;

  WebUiInteractiveTestMixin(const WebUiInteractiveTestMixin&) = delete;
  WebUiInteractiveTestMixin& operator=(const WebUiInteractiveTestMixin&) =
      delete;

  auto WaitForElementToRender(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& element) {
    WebContentsInteractionTestUtil::StateChange element_renders;
    element_renders.event = kElementRenders;
    element_renders.where = element;
    element_renders.test_function =
        "(el) => { if (el !== null) { let rect = el.getBoundingClientRect(); "
        "return rect.width > 0 && rect.height > 0; } return false; }";

    return T::WaitForStateChange(contents_id, element_renders);
  }

  auto ClickElement(const ui::ElementIdentifier& contents_id,
                    const WebContentsInteractionTestUtil::DeepQuery& element) {
    return T::Steps(WaitForElementToRender(contents_id, element),
                    T::ScrollIntoView(contents_id, element),
                    T::MoveMouseTo(contents_id, element), T::ClickMouse());
  }

  auto WaitForButtonStateChange(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& element,
      bool is_checked) {
    WebContentsInteractionTestUtil::StateChange toggle_selection_change;
    toggle_selection_change.event = kButtonWasClicked;
    toggle_selection_change.where = element;
    toggle_selection_change.test_function = base::StrCat(
        {"(el) => { return ", is_checked ? "" : "!", "el.checked; }"});

    return T::WaitForStateChange(contents_id, toggle_selection_change);
  }

  auto WaitForIronListCollapseStateChange(
      const ui::ElementIdentifier& contents_id,
      const WebContentsInteractionTestUtil::DeepQuery& query) {
    WebContentsInteractionTestUtil::StateChange iron_collapse_finish_animating;
    iron_collapse_finish_animating.event = kIronCollapseContentShows;
    iron_collapse_finish_animating.where = query;
    iron_collapse_finish_animating.test_function =
        "(el) => { return !el.transitioning; }";

    return T::WaitForStateChange(contents_id, iron_collapse_finish_animating);
  }
};

#endif  // CHROME_BROWSER_UI_WEBUI_TEST_SUPPORT_WEBUI_INTERACTIVE_TEST_MIXIN_H_
