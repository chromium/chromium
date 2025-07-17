// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "url/gurl.h"

namespace tabs {
class TabInterface;
}

namespace page_actions {

struct PageActionProperties;

// Records visibility metrics for a specific page action, scoped to a single
// ActionId. This class does not handle all page action metrics, only for the
// one it is instantiated for.
class PageActionPerActionMetricsRecorder
    : public PageActionPerActionMetricsRecorderInterface,
      public PageActionModelObserver {
 public:
  PageActionPerActionMetricsRecorder(
      tabs::TabInterface& tab_interface,
      const PageActionProperties& properties,
      PageActionModelInterface& model,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback);

  PageActionPerActionMetricsRecorder(
      const PageActionPerActionMetricsRecorder&) = delete;
  PageActionPerActionMetricsRecorder operator=(
      const PageActionPerActionMetricsRecorder&) = delete;

  ~PageActionPerActionMetricsRecorder() override;

  // PageActionPerActionMetricsRecorderInterface:
  void RecordClick(PageActionTrigger trigger_source) override;

  // PageActionModelObserver
  void OnPageActionModelChanged(const PageActionModelInterface& model) override;
  void OnPageActionModelWillBeDeleted(
      const PageActionModelInterface& model) override;

 private:
  // Indicates the sates of the visual representation currently shown to the
  // user.
  enum class DisplayState { kHidden = 0, kIconOnly = 1, kChip = 2 };

  // Tracks if metrics have been recorded for the current navigation.
  struct NavigationMetrics {
    bool icon_shown_recorded = false;
    bool chip_shown_recorded = false;
    GURL url;
  };

  // Helpers to coordinate the state change.
  void UpdateDisplayState(const PageActionModelInterface& model);

  // Determines the current display state base on the `model`.
  DisplayState DetermineDisplayState(const PageActionModelInterface& model);

  // Detects a new main‑frame navigation.
  //
  // Returns `true` if the URL of the tab’s primary `WebContents` differs
  // from the URL we saw the last time this method was queried. When a new
  // navigation is detected the method resets the per‑navigation flags inside
  // `current_navigation_metrics_` (so subsequent calls during the same page
  // view return false).
  //
  // This helper is called from UpdateDisplayState() right before deciding
  // whether to emit per‑navigation "shown" metrics, ensuring we count each
  // page only once regardless of how many visibility updates arrive.
  bool IsNewNavigation();

  // Helpers to record metrics for each state/action.
  void RecordIconShown();
  void RecordChipShown();
  void RecordIconClick();
  void RecordChipClick();

  // Tracks per-navigation state.
  NavigationMetrics current_navigation_metrics_;

  // Tracks the latest visual state (what the user sees right now).
  DisplayState current_display_state_ = DisplayState::kHidden;

  // Used to get count of visible ephemeral page actions from the
  // `PageActionController`.
  VisibleEphemeralPageActionsCountCallback
      visible_ephemeral_page_actions_count_callback_;

  // Properties associated with the specific page action being observed.
  const PageActionIconType page_action_type_;
  const std::string histogram_name_;

  // The TabInterface is guaranteed valid for this object’s lifetime.
  const raw_ref<tabs::TabInterface> tab_interface_;

  base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>
      scoped_observation_{this};
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_
