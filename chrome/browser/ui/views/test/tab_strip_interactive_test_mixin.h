// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TEST_TAB_STRIP_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_TEST_TAB_STRIP_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/interactive_test.h"

// Template to be used as a mixin class for tab strip tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class TabStripInteractiveTestMixin : public T {
 public:
  template <class... Args>
  explicit TabStripInteractiveTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~TabStripInteractiveTestMixin() override = default;

  TabStripInteractiveTestMixin(const TabStripInteractiveTestMixin&) = delete;
  TabStripInteractiveTestMixin& operator=(const TabStripInteractiveTestMixin&) =
      delete;

  auto FinishTabstripAnimations() {
    return T::Steps(T::WaitForShow(kTabStripElementId),
                    T::WithView(kTabStripElementId, [](TabStrip* tab_strip) {
                      tab_strip->StopAnimating();
                    }));
  }

  auto HoverTabAt(int index) {
    const char kTabToHover[] = "Tab to hover";
    return T::Steps(FinishTabstripAnimations(),
                    T::template NameDescendantViewByType<Tab>(
                        kTabStripElementId, kTabToHover, index),
                    T::MoveMouseTo(kTabToHover));
  }

  auto HoverTabGroupHeader(tab_groups::TabGroupId group_id) {
    const char kTabGroupHeaderToHover[] = "Tab group header to hover";
    return T::Steps(
        FinishTabstripAnimations(),
        T::NameDescendantView(
            kBrowserViewElementId, kTabGroupHeaderToHover,
            base::BindRepeating(
                [](tab_groups::TabGroupId group_id, const views::View* view) {
                  const TabGroupHeader* header =
                      views::AsViewClass<TabGroupHeader>(view);
                  if (!header) {
                    return false;
                  }
                  return header->group().value() == group_id;
                },
                group_id)),
        T::MoveMouseTo(kTabGroupHeaderToHover));
  }

  auto WaitForActiveTabChange(int index) {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                        kTabActiveChangeObserver);
    return T::Steps(
        T::PollState(kTabActiveChangeObserver,
                     base::BindRepeating(
                         &TabStripModel::active_index,
                         base::Unretained(T::browser()->tab_strip_model()))),
        T::WaitForState(kTabActiveChangeObserver, index),
        T::StopObservingState(kTabActiveChangeObserver));
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_TEST_TAB_STRIP_INTERACTIVE_TEST_MIXIN_H_
