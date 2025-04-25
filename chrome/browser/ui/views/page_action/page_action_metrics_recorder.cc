// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/page_action/page_action_enums.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/action_id.h"

namespace page_actions {

PageActionMetricsRecorder::PageActionMetricsRecorder(
    tabs::TabInterface& tab_interface,
    const PageActionProperties& properties,
    PageActionModelInterface& model,
    VisibleEphemeralPageActionsCountCallback
        visible_ephemeral_page_actions_count_callback)
    : is_ephemeral_(properties.is_ephemeral),
      page_action_type_(properties.type),
      histogram_name_(properties.histogram_name),
      visible_ephemeral_page_actions_count_callback_(
          std::move(visible_ephemeral_page_actions_count_callback)),
      tab_interface_(tab_interface) {
  scoped_observation_.Observe(&model);
}

PageActionMetricsRecorder::~PageActionMetricsRecorder() = default;

void PageActionMetricsRecorder::OnPageActionModelChanged(
    const PageActionModelInterface& model) {
  if (model.GetVisible()) {
    OnPageActionVisible();
  }
}

void PageActionMetricsRecorder::OnPageActionModelWillBeDeleted(
    const PageActionModelInterface& model) {
  scoped_observation_.Reset();
}

void PageActionMetricsRecorder::OnPageActionVisible() {
  // Page action can be permanent or ephemeral. For the
  // "PageActionController.ActionTypeShown2" metric, it should be recorded only
  // for when the page action is ephemeral.
  if (!is_ephemeral_) {
    return;
  }

  CHECK(tab_interface_->GetContents());

  // Only record the "Shown" metric the first time the icon appears on a "page".
  const GURL current_url = tab_interface_->GetContents()->GetURL();

  // TODO(crbug.com/407974430): [Metric] Record per-navigation metric for
  // ...ActionTypeShown
  if (page_action_recorded_urls_.contains(current_url)) {
    return;
  }

  page_action_recorded_urls_.insert(current_url);

  base::UmaHistogramEnumeration("PageActionController.Icon.CTR2",
                                PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration(
      base::StrCat({"PageActionController.", histogram_name_, ".Icon.CTR2"}),
      PageActionCTREvent::kShown);
  base::UmaHistogramEnumeration("PageActionController.ActionTypeShown2",
                                page_action_type_);
}

void PageActionMetricsRecorder::RecordClick(
    PageActionTrigger /*trigger_source*/) {
  base::UmaHistogramEnumeration("PageActionController.Icon.CTR2",
                                PageActionCTREvent::kClicked);
  base::UmaHistogramEnumeration(
      base::StrCat({"PageActionController.", histogram_name_, ".Icon.CTR2"}),
      PageActionCTREvent::kClicked);
  base::UmaHistogramExactLinear(
      "PageActionController.Icon.NumberActionsShownWhenClicked",
      visible_ephemeral_page_actions_count_callback_.Run(), 20);
}

}  // namespace page_actions
