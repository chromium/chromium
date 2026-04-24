// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "ui/actions/action_id.h"
#include "url/gurl.h"

namespace tabs {
class TabInterface;
}

namespace page_actions {

struct PageActionProperties;

// Records both per-action and page-level metrics for page actions.
class PageActionMetricsRecorder : public PageActionMetricsRecorderInterface,
                                  public PageActionModelObserver {
 public:
  PageActionMetricsRecorder(tabs::TabInterface& tab_interface,
                            VisibleEphemeralPageActionsCountCallback
                                visible_ephemeral_page_actions_count_callback);

  PageActionMetricsRecorder(const PageActionMetricsRecorder&) = delete;
  PageActionMetricsRecorder operator=(const PageActionMetricsRecorder&) =
      delete;

  ~PageActionMetricsRecorder() override;

  // PageActionMetricsRecorderInterface:
  void RecordClick(actions::ActionId action_id,
                   PageActionTrigger trigger_source) override;
  void Observe(PageActionModelInterface& model,
               const PageActionProperties& properties) override;

  // PageActionModelObserver:
  void OnPageActionModelChanged(const PageActionModelInterface& model) override;
  void OnPageActionModelWillBeDeleted(
      const PageActionModelInterface& model) override;

 private:
  enum class DisplayState { kHidden = 0, kIconOnly = 1, kChip = 2 };

  struct PerActionState {
    PageActionIconType type;
    std::string histogram_name;
    DisplayState display_state = DisplayState::kHidden;
    bool icon_shown_recorded = false;
    bool chip_shown_recorded = false;
    int last_seen_navigation_id = 0;
  };

  void UpdateDisplayState(actions::ActionId action_id,
                          const PageActionModelInterface& model);
  DisplayState DetermineDisplayState(const PageActionModelInterface& model);

  void CheckForNewNavigation();
  void ResetPerActionStateForNewNavigation(actions::ActionId action_id);

  void RecordPageShownMetrics();
  void RecordIconShown(actions::ActionId action_id);
  void RecordChipShown(actions::ActionId action_id);
  void RecordIconClick(actions::ActionId action_id);
  void RecordChipClick(actions::ActionId action_id);
  void RecordShownPerNavigation(actions::ActionId action_id);

  const raw_ref<tabs::TabInterface> tab_interface_;
  VisibleEphemeralPageActionsCountCallback
      visible_ephemeral_page_actions_count_callback_;

  GURL last_committed_url_;
  int current_navigation_id_ = 0;

  bool has_recorded_page_shown_for_navigation_ = false;
  bool has_recorded_action_shown_for_navigation_ = false;
  bool has_recorded_multi_shown_for_navigation_ = false;
  int last_recorded_count_ = 0;

  std::map<actions::ActionId, PerActionState> per_action_states_;

  base::ScopedMultiSourceObservation<PageActionModelInterface,
                                     PageActionModelObserver>
      model_observations_{this};
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_
