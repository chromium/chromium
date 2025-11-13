// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/views/page_action/page_action_enums.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/action_id.h"

namespace page_actions {

PageActionPerActionMetricsRecorder::PageActionPerActionMetricsRecorder(
    tabs::TabInterface& tab_interface,
    const PageActionProperties& properties,
    PageActionModelInterface& model,
    VisibleEphemeralPageActionsCountCallback
        visible_ephemeral_page_actions_count_callback)
    : visible_ephemeral_page_actions_count_callback_(
          std::move(visible_ephemeral_page_actions_count_callback)),
      page_action_type_(properties.type),
      histogram_name_(properties.histogram_name),
      tab_interface_(tab_interface) {
  scoped_observation_.Observe(&model);
}

PageActionPerActionMetricsRecorder::~PageActionPerActionMetricsRecorder() =
    default;

void PageActionPerActionMetricsRecorder::OnPageActionModelChanged(
    const PageActionModelInterface& model) {
  if (IsNewNavigation()) {
    current_navigation_metrics_.icon_shown_recorded = false;
    current_navigation_metrics_.chip_shown_recorded = false;
  }

  UpdateDisplayState(model);
}

void PageActionPerActionMetricsRecorder::OnPageActionModelWillBeDeleted(
    const PageActionModelInterface& model) {
  scoped_observation_.Reset();
}

PageActionPerActionMetricsRecorder::DisplayState
PageActionPerActionMetricsRecorder::DetermineDisplayState(
    const PageActionModelInterface& model) {
  if (!model.GetVisible()) {
    return DisplayState::kHidden;
  }

  if (model.IsChipShowing()) {
    return DisplayState::kChip;
  }

  return DisplayState::kIconOnly;
}

void PageActionPerActionMetricsRecorder::UpdateDisplayState(
    const PageActionModelInterface& model) {
  DisplayState previous_display_state = current_display_state_;
  current_display_state_ = DetermineDisplayState(model);

  // The model updates may trigger this method. This check will prevent multiple
  // metrics record for the same state.
  if (previous_display_state == current_display_state_) {
    return;
  }

  // Always record icon shown when transitioning from hidden.
  if (previous_display_state == DisplayState::kHidden) {
    RecordIconShown();
  }

  // Always record chip when transitioning to chip.
  if (current_display_state_ == DisplayState::kChip) {
    RecordChipShown();
  }

  // TODO(https://crbug.com/376285067): Record chip contention metrics.
}

bool PageActionPerActionMetricsRecorder::IsNewNavigation() {
  content::WebContents* contents = tab_interface_->GetContents();
  if (!contents) {
    return false;
  }

  // TODO(crbug.com/407974430): [Metric] Record per-navigation metric for
  // ...ActionTypeShown
  const GURL current_url = contents->GetURL();
  if (current_url != current_navigation_metrics_.url) {
    current_navigation_metrics_.url = current_url;
    return true;
  }

  return false;
}

void PageActionPerActionMetricsRecorder::RecordIconShown() {
  if (current_navigation_metrics_.icon_shown_recorded) {
    return;
  }

  current_navigation_metrics_.icon_shown_recorded = true;

  base::UmaHistogramEnumeration("PageActionController.Icon.CTR2",
                                PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration(
      base::StrCat({"PageActionController.", histogram_name_, ".Icon.CTR2"}),
      PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration("PageActionController.ActionTypeShown2",
                                page_action_type_);
}

void PageActionPerActionMetricsRecorder::RecordChipShown() {
  if (current_navigation_metrics_.chip_shown_recorded) {
    return;
  }

  current_navigation_metrics_.chip_shown_recorded = true;

  base::UmaHistogramEnumeration("PageActionController.Chip.CTR2",
                                PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration(
      base::StrCat({"PageActionController.", histogram_name_, ".Chip.CTR2"}),
      PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration("PageActionController.ChipTypeShown",
                                page_action_type_);
}

void PageActionPerActionMetricsRecorder::RecordClick(
    PageActionTrigger trigger_source) {
  switch (current_display_state_) {
    case DisplayState::kChip:
      RecordChipClick();
      break;
    case DisplayState::kIconOnly:
      RecordIconClick();
      break;
    case DisplayState::kHidden:
    default:
      // If hidden, we shouldn't get clicks event.
      NOTREACHED();
  }
}

void PageActionPerActionMetricsRecorder::RecordIconClick() {
  base::UmaHistogramEnumeration("PageActionController.Icon.CTR2",
                                PageActionCTREvent::kClicked);
  base::UmaHistogramEnumeration(
      base::StrCat({"PageActionController.", histogram_name_, ".Icon.CTR2"}),
      PageActionCTREvent::kClicked);
  base::UmaHistogramExactLinear(
      "PageActionController.Icon.NumberActionsShownWhenClicked",
      visible_ephemeral_page_actions_count_callback_.Run(), 20);
}

void PageActionPerActionMetricsRecorder::RecordChipClick() {
  base::UmaHistogramEnumeration("PageActionController.Chip.CTR2",
                                PageActionCTREvent::kClicked);
  base::UmaHistogramEnumeration(
      base::StrCat({"PageActionController.", histogram_name_, ".Chip.CTR2"}),
      PageActionCTREvent::kClicked);
  base::UmaHistogramExactLinear(
      "PageActionController.Chip.NumberActionsShownWhenClicked",
      visible_ephemeral_page_actions_count_callback_.Run(), 20);
}

}  // namespace page_actions
