// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/filter_acceptance_metrics_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/multistep_filter/core/logging/multistep_filter_metrics.h"

namespace multistep_filter {

namespace {

void LogAcceptanceHistogram(std::string_view base_histogram,
                            std::string_view task_type,
                            SuggestionUserDecision decision) {
  base::UmaHistogramEnumeration(std::string(base_histogram), decision);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {base_histogram, kMultistepFilterByTaskHistogramPrefix, task_type}),
      decision);
}

}  // namespace

FilterAcceptanceMetricsLogger::FilterAcceptanceMetricsLogger(
    std::string_view task_type)
    : task_type_(task_type) {}

FilterAcceptanceMetricsLogger::~FilterAcceptanceMetricsLogger() {
  if (last_decision_from_initial_cue_) {
    LogAcceptanceHistogram(kMultistepFilterAcceptanceInitialCueHistogram,
                           task_type_, *last_decision_from_initial_cue_);
  }
  if (last_decision_from_reopened_cue_) {
    LogAcceptanceHistogram(kMultistepFilterAcceptanceReopenedCueHistogram,
                           task_type_, *last_decision_from_reopened_cue_);
  }
  if (last_decision_) {
    LogAcceptanceHistogram(kMultistepFilterAcceptanceHistogram, task_type_,
                           *last_decision_);
  }
}

void FilterAcceptanceMetricsLogger::RecordInitialCueShown() {
  last_decision_from_initial_cue_ = SuggestionUserDecision::kIgnored;
  last_decision_ = SuggestionUserDecision::kIgnored;
}

void FilterAcceptanceMetricsLogger::RecordReopenedCueShown() {
  if (!last_decision_from_reopened_cue_) {
    last_decision_from_reopened_cue_ = SuggestionUserDecision::kIgnored;
    last_decision_ = SuggestionUserDecision::kIgnored;
  }
}

void FilterAcceptanceMetricsLogger::RecordInitialCueAndOverallDecision(
    SuggestionUserDecision decision) {
  last_decision_from_initial_cue_ = decision;
  last_decision_ = decision;
}

void FilterAcceptanceMetricsLogger::RecordReopenedCueAndOverallDecision(
    SuggestionUserDecision decision) {
  last_decision_from_reopened_cue_ = decision;
  last_decision_ = decision;
}

}  // namespace multistep_filter
