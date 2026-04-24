// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_enums.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/action_id.h"

namespace page_actions {

namespace {

inline void RecordPageEvent(PageActionPageEvent event) {
  base::UmaHistogramEnumeration("PageActionController.PagesWithActionsShown3",
                                event);
}

}  // namespace

PageActionMetricsRecorder::PageActionMetricsRecorder(
    tabs::TabInterface& tab_interface,
    VisibleEphemeralPageActionsCountCallback
        visible_ephemeral_page_actions_count_callback)
    : tab_interface_(tab_interface),
      visible_ephemeral_page_actions_count_callback_(
          std::move(visible_ephemeral_page_actions_count_callback)) {}

PageActionMetricsRecorder::~PageActionMetricsRecorder() {
  for (auto& entry : per_action_states_) {
    RecordShownPerNavigation(entry.first);
  }
}

void PageActionMetricsRecorder::RecordClick(actions::ActionId action_id,
                                            PageActionTrigger trigger_source) {
  auto it = per_action_states_.find(action_id);
  if (it == per_action_states_.end()) {
    return;
  }

  switch (it->second.display_state) {
    case DisplayState::kChip:
      RecordChipClick(action_id);
      break;
    case DisplayState::kIconOnly:
      RecordIconClick(action_id);
      break;
    case DisplayState::kHidden:
      // This can happen if a click event is processed after a navigation
      // starts but before the model has notified the recorder of its new
      // state.
      break;
  }
}

void PageActionMetricsRecorder::Observe(
    PageActionModelInterface& model,
    const PageActionProperties& properties) {
  actions::ActionId action_id = model.GetActionId();
  auto& state = per_action_states_[action_id];
  state.type = properties.type;
  state.histogram_name = properties.histogram_name;

  model_observations_.AddObservation(&model);
}

void PageActionMetricsRecorder::OnPageActionModelChanged(
    const PageActionModelInterface& model) {
  CheckForNewNavigation();

  actions::ActionId action_id = model.GetActionId();

  // Lazily reset per-action state if this action is seeing the new navigation
  // for the first time.
  auto it = per_action_states_.find(action_id);
  if (it != per_action_states_.end() &&
      it->second.last_seen_navigation_id < current_navigation_id_) {
    ResetPerActionStateForNewNavigation(action_id);
  }

  // Page-level metrics: only react to *ephemeral* changes.
  if (model.IsEphemeral()) {
    const int count = visible_ephemeral_page_actions_count_callback_.Run();

    if (count != last_recorded_count_) {
      last_recorded_count_ = count;
      if (count > 0) {
        // 20 chosen as an upper bound for simultaneous actions.
        base::UmaHistogramExactLinear(
            "PageActionController.NumberActionsShown3", count, 20);
      }
    }

    if (model.GetVisible()) {
      RecordPageShownMetrics();

      if (count > 1 && !has_recorded_multi_shown_for_navigation_) {
        RecordPageEvent(PageActionPageEvent::kMultipleActionsShown);
        has_recorded_multi_shown_for_navigation_ = true;
      }
    }
  }

  // Per-action metrics.
  UpdateDisplayState(action_id, model);
}

void PageActionMetricsRecorder::OnPageActionModelWillBeDeleted(
    const PageActionModelInterface& model) {
  actions::ActionId action_id = model.GetActionId();
  RecordShownPerNavigation(action_id);
  model_observations_.RemoveObservation(
      const_cast<PageActionModelInterface*>(&model));
  per_action_states_.erase(action_id);
}

PageActionMetricsRecorder::DisplayState
PageActionMetricsRecorder::DetermineDisplayState(
    const PageActionModelInterface& model) {
  if (!model.GetVisible()) {
    return DisplayState::kHidden;
  }

  if (model.IsChipShowing()) {
    return DisplayState::kChip;
  }

  return DisplayState::kIconOnly;
}

void PageActionMetricsRecorder::UpdateDisplayState(
    actions::ActionId action_id,
    const PageActionModelInterface& model) {
  auto it = per_action_states_.find(action_id);
  CHECK(it != per_action_states_.end());
  auto& state = it->second;

  DisplayState previous_display_state = state.display_state;
  state.display_state = DetermineDisplayState(model);

  if (previous_display_state == state.display_state) {
    return;
  }

  // Always record icon shown when transitioning from hidden.
  if (previous_display_state == DisplayState::kHidden) {
    RecordIconShown(action_id);
  }

  // Always record chip when transitioning to chip.
  if (state.display_state == DisplayState::kChip) {
    RecordChipShown(action_id);
  }
}

void PageActionMetricsRecorder::ResetPerActionStateForNewNavigation(
    actions::ActionId action_id) {
  auto it = per_action_states_.find(action_id);
  CHECK(it != per_action_states_.end());

  // Record metrics for the navigation we are leaving.
  RecordShownPerNavigation(action_id);

  // Reset state for the new navigation.
  it->second.icon_shown_recorded = false;
  it->second.chip_shown_recorded = false;
  it->second.display_state = DisplayState::kHidden;
  it->second.last_seen_navigation_id = current_navigation_id_;
}

void PageActionMetricsRecorder::CheckForNewNavigation() {
  content::WebContents* contents = tab_interface_->GetContents();
  if (!contents) {
    return;
  }

  const GURL current_url = contents->GetURL();
  if (current_url != last_committed_url_ && !current_url.is_empty()) {
    last_committed_url_ = current_url;

    // Incrementing the navigation ID triggers a lazy reset for each action the
    // next time it is updated.
    current_navigation_id_++;

    // Reset page-level metrics state.
    has_recorded_page_shown_for_navigation_ = false;
    has_recorded_action_shown_for_navigation_ = false;
    has_recorded_multi_shown_for_navigation_ = false;
    last_recorded_count_ = 0;
  }
}

void PageActionMetricsRecorder::RecordPageShownMetrics() {
  if (!has_recorded_page_shown_for_navigation_) {
    RecordPageEvent(PageActionPageEvent::kPageShown);
    has_recorded_page_shown_for_navigation_ = true;
  }
  if (!has_recorded_action_shown_for_navigation_) {
    RecordPageEvent(PageActionPageEvent::kActionShown);
    has_recorded_action_shown_for_navigation_ = true;
  }
}

void PageActionMetricsRecorder::RecordIconShown(actions::ActionId action_id) {
  auto it = per_action_states_.find(action_id);
  CHECK(it != per_action_states_.end());
  auto& state = it->second;

  if (state.icon_shown_recorded) {
    return;
  }

  state.icon_shown_recorded = true;

  base::UmaHistogramEnumeration("PageActionController.Icon.CTR2",
                                PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"PageActionController.", state.histogram_name, ".Icon.CTR2"}),
      PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration("PageActionController.ActionTypeShown2",
                                state.type);
}

void PageActionMetricsRecorder::RecordChipShown(actions::ActionId action_id) {
  auto it = per_action_states_.find(action_id);
  CHECK(it != per_action_states_.end());
  auto& state = it->second;

  if (state.chip_shown_recorded) {
    return;
  }

  state.chip_shown_recorded = true;

  base::UmaHistogramEnumeration("PageActionController.Chip.CTR2",
                                PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"PageActionController.", state.histogram_name, ".Chip.CTR2"}),
      PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration("PageActionController.ChipTypeShown",
                                state.type);
}

void PageActionMetricsRecorder::RecordIconClick(actions::ActionId action_id) {
  auto it = per_action_states_.find(action_id);
  CHECK(it != per_action_states_.end());
  auto& state = it->second;

  base::UmaHistogramEnumeration("PageActionController.Icon.CTR2",
                                PageActionCTREvent::kClicked);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"PageActionController.", state.histogram_name, ".Icon.CTR2"}),
      PageActionCTREvent::kClicked);
  base::UmaHistogramExactLinear(
      "PageActionController.Icon.NumberActionsShownWhenClicked",
      visible_ephemeral_page_actions_count_callback_.Run(), 20);
}

void PageActionMetricsRecorder::RecordChipClick(actions::ActionId action_id) {
  auto it = per_action_states_.find(action_id);
  CHECK(it != per_action_states_.end());
  auto& state = it->second;

  base::UmaHistogramEnumeration("PageActionController.Chip.CTR2",
                                PageActionCTREvent::kClicked);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"PageActionController.", state.histogram_name, ".Chip.CTR2"}),
      PageActionCTREvent::kClicked);
  base::UmaHistogramExactLinear(
      "PageActionController.Chip.NumberActionsShownWhenClicked",
      visible_ephemeral_page_actions_count_callback_.Run(), 20);
}

void PageActionMetricsRecorder::RecordShownPerNavigation(
    actions::ActionId action_id) {
  auto it = per_action_states_.find(action_id);
  if (it == per_action_states_.end()) {
    return;
  }

  // Do not record if the action hasn't been tracked during a valid navigation
  // yet.
  if (it->second.last_seen_navigation_id == 0) {
    return;
  }

  bool was_shown =
      it->second.icon_shown_recorded || it->second.chip_shown_recorded;
  base::UmaHistogramBoolean(
      base::StrCat({"PageActionController.", it->second.histogram_name,
                    ".ShownPerNavigation"}),
      was_shown);
}

}  // namespace page_actions
