// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_TAB_HELPER_H_

#include "base/byte_count.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host.h"

namespace memory_saver {
enum class ChipState {
  // Chip is not shown for this tab.
  HIDDEN,
  // Chip is rendered in a collapsed state with not text.
  COLLAPSED,
  // Chip expands to educate about the memory saver feature.
  EXPANDED_EDUCATION,
  // Chip expands to highlight large savings from a previously discarded tab.
  EXPANDED_WITH_SAVINGS,
  // Previously expanded chip is collapsed due to tab switching.
  COLLAPSED_FROM_EXPANDED,
};
}  // namespace memory_saver

// When a page in the background has been discarded due to memory saver mode,
// and the user returns to that tab, a page action chip should be shown to the
// user which conveys information about the discarded tab to the user.
// The MemorySaverChipTabHelper is a per-tab class which manages the state of
// the memory saver chip.
class MemorySaverChipTabHelper : public tabs::ContentsObservingTabFeature,
                                 public performance_manager::user_tuning::
                                     UserPerformanceTuningManager::Observer {
 public:
  explicit MemorySaverChipTabHelper(tabs::TabInterface& tab);
  ~MemorySaverChipTabHelper() override;

  static constexpr int kChipAnimationCount = 3;

  memory_saver::ChipState chip_state() const { return chip_state_; }

  // content::WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // performance_manager::user_tuning::UserPerformanceTuningManager::Observer:
  // Checks whether memory saver mode is currently enabled.
  void OnMemorySaverModeChanged() override;

  // Returns whether the tab associated with this helper has been navigated
  // away from and to another tab.
  bool ShouldChipAnimate();

 private:
  // Threshold was selected based on the 75th percentile of tab memory usage.
  static constexpr base::ByteCount kExpandedMemorySaverChipThreshold =
      base::MiB(197);

  static constexpr base::TimeDelta kExpandedMemorySaverChipFrequency =
      base::Days(1);
  static constexpr base::TimeDelta kExpandedMemorySaverChipDiscardedDuration =
      base::Hours(3);

  // Checks whether a promotional expanded chip should be shown to highlight
  // memory savings and, if so, update prefs to reflect that it is shown.
  bool ComputeShouldHighlightMemorySavings();

  // Checks whether an educational expanded chip should be shown about the
  // feature and, if so, update prefs to reflect that it is shown.
  bool ComputeShouldEducateAboutMemorySavings();

  // Computes and updates the `chip_state_` based on information about the
  // recent navigation.
  void ComputeChipState(content::NavigationHandle* navigation_handle);

  // Applies the computed page action icon or chip state to the new page action
  // framework.
  void UpdatePageActionState();

  memory_saver::ChipState chip_state_ = memory_saver::ChipState::HIDDEN;
  // Represents whether the current chip state has been properly rendered. This
  // gets reset when a tab gets hidden so the chip can be redrawn.
  bool was_rendered_ = false;
  raw_ptr<PrefService> pref_service_;

  // Used to track whether MemorySaver mode is enabled, and hence whether
  // related UI should be surfaced.
  base::ScopedObservation<
      performance_manager::user_tuning::UserPerformanceTuningManager,
      performance_manager::user_tuning::UserPerformanceTuningManager::Observer>
      user_performance_tuning_manager_observation_{this};
  bool is_memory_saver_mode_enabled_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_TAB_HELPER_H_
