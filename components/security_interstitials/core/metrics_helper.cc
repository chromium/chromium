// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/metrics_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/history/core/browser/history_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using base::RecordAction;
using base::UserMetricsAction;

namespace security_interstitials {

namespace {

// Directly adds to the UMA histograms, using the same properties as
// UMA_HISTOGRAM_ENUMERATION, because the macro doesn't allow non-constant
// histogram names.
void RecordSingleDecisionToMetrics(MetricsHelper::Decision decision,
                                   const std::string& histogram_name) {
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, MetricsHelper::MAX_DECISION,
      MetricsHelper::MAX_DECISION + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(decision);
}

void RecordSingleInteractionToMetrics(MetricsHelper::Interaction interaction,
                                      const std::string& histogram_name) {
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, MetricsHelper::MAX_INTERACTION,
      MetricsHelper::MAX_INTERACTION + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(interaction);
}

void MaybeRecordDecisionAsAction(MetricsHelper::Decision decision,
                                 const std::string& metric_name) {
  if (decision == MetricsHelper::PROCEED) {
    if (metric_name == "malware")
      RecordAction(UserMetricsAction("MalwareInterstitial.Proceed"));
    else if (metric_name == "harmful")
      RecordAction(UserMetricsAction("HarmfulInterstitial.Proceed"));
    else if (metric_name == "ssl_overridable")
      RecordAction(UserMetricsAction("SSLOverridableInterstitial.Proceed"));
  } else if (decision == MetricsHelper::DONT_PROCEED) {
    if (metric_name == "malware")
      RecordAction(UserMetricsAction("MalwareInterstitial.Back"));
    else if (metric_name == "harmful")
      RecordAction(UserMetricsAction("HarmfulInterstitial.Back"));
    else if (metric_name == "ssl_overridable")
      RecordAction(UserMetricsAction("SSLOverridableInterstitial.Back"));
    else if (metric_name == "ssl_nonoverridable")
      RecordAction(UserMetricsAction("SSLNonOverridableInsterstitial.Back"));
    else if (metric_name == "bad_clock")
      RecordAction(UserMetricsAction("BadClockInterstitial.Back"));
  }
}

void MaybeRecordInteractionAsAction(MetricsHelper::Interaction interaction,
                                    const std::string& metric_name) {
  if (interaction == MetricsHelper::TOTAL_VISITS) {
    if (metric_name == "malware")
      RecordAction(UserMetricsAction("MalwareInterstitial.Show"));
    else if (metric_name == "harmful")
      RecordAction(UserMetricsAction("HarmfulInterstitial.Show"));
    else if (metric_name == "ssl_overridable")
      RecordAction(UserMetricsAction("SSLOverridableInterstitial.Show"));
    else if (metric_name == "ssl_nonoverridable")
      RecordAction(UserMetricsAction("SSLNonOverridableInterstitial.Show"));
    else if (metric_name == "bad_clock")
      RecordAction(UserMetricsAction("BadClockInterstitial.Show"));
  } else if (interaction == MetricsHelper::SHOW_ADVANCED) {
    if (metric_name == "malware") {
      RecordAction(UserMetricsAction("MalwareInterstitial.Advanced"));
    } else if (metric_name == "harmful") {
      RecordAction(UserMetricsAction("HarmfulInterstitial.Advanced"));
    } else if (metric_name == "ssl_overridable" ||
               metric_name == "ssl_nonoverridable") {
      RecordAction(UserMetricsAction("SSLInterstitial.Advanced"));
    }
  } else if (interaction == MetricsHelper::RELOAD) {
    if (metric_name == "ssl_nonoverridable")
      RecordAction(UserMetricsAction("SSLInterstitial.Reload"));
  } else if (interaction == MetricsHelper::OPEN_TIME_SETTINGS) {
    if (metric_name == "bad_clock")
      RecordAction(UserMetricsAction("BadClockInterstitial.Settings"));
  }
}

}  // namespace

MetricsHelper::~MetricsHelper() {}

MetricsHelper::ReportDetails::ReportDetails() {}

MetricsHelper::ReportDetails::ReportDetails(const ReportDetails& other) =
    default;

MetricsHelper::ReportDetails::~ReportDetails() {}

MetricsHelper::MetricsHelper(const GURL& request_url,
                             const ReportDetails settings,
                             history::HistoryService* history_service)
    : request_url_(request_url), settings_(settings), num_visits_(-1) {
  DCHECK(!settings_.metric_prefix.empty());
  if (history_service) {
    history_service->GetVisibleVisitCountToHost(
        request_url_,
        base::BindOnce(&MetricsHelper::OnGotHistoryCount,
                       base::Unretained(this)),
        &request_tracker_);
  }
}

void MetricsHelper::RecordUserDecision(Decision decision) {
  const std::string histogram_name(
      "interstitial." + settings_.metric_prefix + ".decision");
  RecordUserDecisionToMetrics(decision, histogram_name);
  // Record additional information about sites that users have visited before.
  // Report |decision| and SHOW together, filtered by the same history state
  // so they they are paired regardless of when if num_visits_ is populated.
  if (num_visits_ > 0 && (decision == PROCEED || decision == DONT_PROCEED)) {
    RecordUserDecisionToMetrics(SHOW, histogram_name + ".repeat_visit");
    RecordUserDecisionToMetrics(decision, histogram_name + ".repeat_visit");
  }

  MaybeRecordDecisionAsAction(decision, settings_.metric_prefix);
}

void MetricsHelper::RecordUserDecisionToMetrics(
    Decision decision,
    const std::string& histogram_name) {
  // Record the decision, and additionally |with extra_suffix|.
  RecordSingleDecisionToMetrics(decision, histogram_name);
  if (!settings_.extra_suffix.empty()) {
    RecordSingleDecisionToMetrics(
        decision, histogram_name + "." + settings_.extra_suffix);
  }
}

void MetricsHelper::RecordUserInteraction(Interaction interaction) {
  const std::string histogram_name(
      "interstitial." + settings_.metric_prefix + ".interaction");
  RecordSingleInteractionToMetrics(interaction, histogram_name);
  if (!settings_.extra_suffix.empty()) {
    RecordSingleInteractionToMetrics(
        interaction, histogram_name + "." + settings_.extra_suffix);
  }

  MaybeRecordInteractionAsAction(interaction, settings_.metric_prefix);
}

void MetricsHelper::RecordShutdownMetrics() {
  RecordExtraShutdownMetrics();
}

int MetricsHelper::NumVisits() {
  return num_visits_;
}

void MetricsHelper::RecordExtraUserDecisionMetrics(Decision decision) {}

void MetricsHelper::RecordExtraUserInteractionMetrics(Interaction interaction) {
}

void MetricsHelper::RecordExtraShutdownMetrics() {}

void MetricsHelper::OnGotHistoryCount(
    history::VisibleVisitCountToHostResult result) {
  if (result.success)
    num_visits_ = result.count;
}

}  // namespace security_interstitials
