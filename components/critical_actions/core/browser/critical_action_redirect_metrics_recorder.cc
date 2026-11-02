// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_redirect_metrics_recorder.h"

#include <string_view>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/critical_actions/core/browser/critical_action_metrics.h"
#include "components/history/core/browser/history_types.h"
#include "ui/base/page_transition_types.h"

namespace critical_actions {

namespace {

bool IsRedirectChainStart(ui::PageTransition transition) {
  return (transition & ui::PAGE_TRANSITION_CHAIN_START) != 0;
}

bool IsRedirectChainContinuation(ui::PageTransition transition) {
  return ui::PageTransitionIsRedirect(transition) &&
         !IsRedirectChainStart(transition);
}

bool IsServerRedirect(ui::PageTransition transition) {
  return (transition & ui::PAGE_TRANSITION_SERVER_REDIRECT) != 0;
}

}  // namespace

CriticalActionRedirectMetricsRecorder::CriticalActionRedirectMetricsRecorder(
    size_t cache_capacity)
    : visits_to_actions_cache_(cache_capacity),
      visit_redirect_types_(cache_capacity) {}

CriticalActionRedirectMetricsRecorder::
    ~CriticalActionRedirectMetricsRecorder() = default;

void CriticalActionRedirectMetricsRecorder::OnURLVisited(
    const history::VisitRow& visit_row) {
  const int64_t referring_visit_id = visit_row.referring_visit;
  const ui::PageTransition transition = visit_row.transition;

  if (referring_visit_id == history::kInvalidVisitID ||
      !IsRedirectChainContinuation(transition)) {
    return;
  }

  // LINT.IfChange(RedirectTypeClassification)
  const RedirectType redirect_type = IsServerRedirect(transition)
                                         ? RedirectType::kServer
                                         : RedirectType::kClient;
  // LINT.ThenChange(//components/critical_actions/core/browser/critical_action_redirect_metrics_recorder.h:RedirectType)

  // Remember the redirect so critical actions arriving for
  // `referring_visit_id` later are recorded immediately.
  visit_redirect_types_.Put(referring_visit_id, redirect_type);

  auto it = visits_to_actions_cache_.Peek(referring_visit_id);
  if (it == visits_to_actions_cache_.end()) {
    return;
  }
  for (const auto& action : it->second) {
    RecordEventLoggedInRedirectHistogram(redirect_type, action.action_source,
                                         action.action_type);
  }
  visits_to_actions_cache_.Erase(it);
}

void CriticalActionRedirectMetricsRecorder::OnActionResolvedToVisit(
    int64_t visit_id,
    ActionSource source,
    ActionType action_type) {
  auto redirected_it = visit_redirect_types_.Peek(visit_id);
  if (redirected_it != visit_redirect_types_.end()) {
    RecordEventLoggedInRedirectHistogram(redirected_it->second, source,
                                         action_type);
    return;
  }

  // Defer recording for visits not yet known to be redirected, in case a
  // subsequent redirect hop from this visit is seen.
  auto it = visits_to_actions_cache_.Get(visit_id);
  if (it == visits_to_actions_cache_.end()) {
    it = visits_to_actions_cache_.Put(visit_id, std::vector<VisitAction>());
  }
  it->second.push_back(VisitAction{source, action_type});
}

void CriticalActionRedirectMetricsRecorder::Clear() {
  visits_to_actions_cache_.Clear();
  visit_redirect_types_.Clear();
}

// static
void CriticalActionRedirectMetricsRecorder::
    RecordEventLoggedInRedirectHistogram(RedirectType redirect_type,
                                         ActionSource source,
                                         ActionType action_type) {
  std::string_view redirect_type_suffix;
  switch (redirect_type) {
    case RedirectType::kServer:
      redirect_type_suffix = "ServerRedirect";
      break;
    case RedirectType::kClient:
      redirect_type_suffix = "ClientRedirect";
      break;
  }
  // Defensive check: guards against values outside the enum, which switch
  // cannot catch, in case future changes introduce new enum values.
  CHECK(!redirect_type_suffix.empty());
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.EventLoggedInRedirect.",
                    redirect_type_suffix, ".",
                    ActionSourceToHistogramSuffix(source)}),
      action_type);
}

}  // namespace critical_actions
