// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_UNIT_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_UNIT_TEST_MIXIN_H_

#include <concepts>

#include "chrome/browser/ui/performance_controls/memory_saver_chip_tab_helper.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/performance_controls/test_support/discard_mock_navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"

// Template to be used as a mixin class for memory saver tests extending
// TestWithBrowserView.
template <typename T>
  requires(std::derived_from<T, TestWithBrowserView>)
class MemorySaverUnitTestMixin : public T {
 public:
  template <class... Args>
  explicit MemorySaverUnitTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~MemorySaverUnitTestMixin() override = default;

  MemorySaverUnitTestMixin(const MemorySaverUnitTestMixin&) = delete;
  MemorySaverUnitTestMixin& operator=(const MemorySaverUnitTestMixin&) = delete;

  void SetMemorySaverModeEnabled(bool enabled) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetMemorySaverModeEnabled(enabled);
  }

  // Creates a new tab at index 0 that would report the given memory savings and
  // discard reason if the tab was discarded
  void AddNewTab(int memory_savings,
                 mojom::LifecycleUnitDiscardReason discard_reason) {
    T::AddTab(T::browser(), GURL("http://foo.com"));
    content::WebContents* const contents =
        T::browser()->tab_strip_model()->GetActiveWebContents();
    MemorySaverChipTabHelper::CreateForWebContents(contents);
    performance_manager::user_tuning::UserPerformanceTuningManager::
        PreDiscardResourceUsage::CreateForWebContents(contents, memory_savings,
                                                      discard_reason);
  }

  void SetTabDiscardState(int tab_index, bool is_discarded) {
    content::WebContents* const web_contents =
        T::browser()->tab_strip_model()->GetWebContentsAt(tab_index);
    std::unique_ptr<DiscardMockNavigationHandle> navigation_handle =
        std::make_unique<DiscardMockNavigationHandle>();
    navigation_handle.get()->SetWasDiscarded(is_discarded);
    navigation_handle.get()->SetWebContents(web_contents);
    MemorySaverChipTabHelper::FromWebContents(web_contents)
        ->DidStartNavigation(navigation_handle.get());

    T::browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->UpdateAll();
  }

  PageActionIconView* GetPageActionIconView() {
    return T::browser_view()
        ->GetLocationBarView()
        ->page_action_icon_controller()
        ->GetIconView(PageActionIconType::kMemorySaver);
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_UNIT_TEST_MIXIN_H_
