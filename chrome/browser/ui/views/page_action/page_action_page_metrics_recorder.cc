// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_page_metrics_recorder.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/page_action/page_action_enums.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace page_actions {

namespace {

inline void RecordPageEvent(PageActionPageEvent event) {
  base::UmaHistogramEnumeration("PageActionController.PagesWithActionsShown3",
                                event);
}

}  // namespace

PageActionPageMetricsRecorder::PageActionPageMetricsRecorder(
    tabs::TabInterface& tab_interface,
    base::RepeatingCallback<int()> visible_ephemeral_actions_count_callback)
    : tab_interface_(tab_interface),
      visible_ephemeral_actions_count_callback_(
          std::move(visible_ephemeral_actions_count_callback)) {}

PageActionPageMetricsRecorder::~PageActionPageMetricsRecorder() = default;

void PageActionPageMetricsRecorder::Observe(PageActionModelInterface& model) {
  model_observations_.AddObservation(&model);
}

void PageActionPageMetricsRecorder::OnPageActionModelChanged(
    const PageActionModelInterface& model) {
  // Only react to *visible* *ephemeral* changes.
  if (!model.GetVisible() || !model.IsEphemeral()) {
    return;
  }

  MaybeRecordPageShownMetrics();

  const int count = visible_ephemeral_actions_count_callback_.Run();
  // 20 chosen as an upper bound for simultaneous actions.
  base::UmaHistogramExactLinear("PageActionController.NumberActionsShown3",
                                count, 20);

  if (count > 1 && !has_recorded_multi_shown_for_navigation_) {
    RecordPageEvent(PageActionPageEvent::kMultipleActionsShown);
    has_recorded_multi_shown_for_navigation_ = true;
  }
}

void PageActionPageMetricsRecorder::OnPageActionModelWillBeDeleted(
    const PageActionModelInterface& model) {
  model_observations_.RemoveObservation(
      const_cast<PageActionModelInterface*>(&model));
}

void PageActionPageMetricsRecorder::MaybeRecordPageShownMetrics() {
  content::WebContents* contents = tab_interface_->GetContents();
  if (!contents) {
    return;
  }

  const GURL url = contents->GetURL();
  if (url != last_committed_url_) {
    RecordPageEvent(PageActionPageEvent::kPageShown);
    // New navigation → reset state.
    last_committed_url_ = url;
    has_recorded_action_shown_for_navigation_ = false;
    has_recorded_multi_shown_for_navigation_ = false;
  }

  if (!has_recorded_action_shown_for_navigation_) {
    RecordPageEvent(PageActionPageEvent::kActionShown);
    has_recorded_action_shown_for_navigation_ = true;
  }
}

}  // namespace page_actions
