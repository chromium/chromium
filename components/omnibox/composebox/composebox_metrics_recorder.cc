// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"

namespace {
const char kComposeboxSessionDuration[] = "Composebox.Session.Duration.Total";
const char kComposeboxSessionCompletionDuration[] =
    "Composebox.Session.Duration.Completed";
const char kComposeboxSessionAbandonedDuration[] =
    "Composebox.Session.Duration.Abandoned";
const char kComposeboxQuerySubmissionTime[] =
    "Composebox.Query.Time.ToSubmission";
}  // namespace

SessionMetrics::SessionMetrics() = default;
SessionMetrics::~SessionMetrics() = default;

ComposeboxMetricsRecorder::ComposeboxMetricsRecorder(
    std::string metric_category_name)
    : metric_category_name_(metric_category_name),
      session_metrics_{std::make_unique<SessionMetrics>()} {}

ComposeboxMetricsRecorder::~ComposeboxMetricsRecorder() {
  // Record session abandonments and completions.
  if (session_state_ == SessionState::kSessionStarted) {
    RecordSessionAbandonedMetrics();
  } else if (session_state_ == SessionState::kNavigationOccurred) {
    RecordSessionCompletedMetrics();
  }
}

void ComposeboxMetricsRecorder::NotifySessionStateChanged(
    SessionState session_state) {
  session_state_ = session_state;
  switch (session_state) {
    case SessionState::kSessionStarted:
      NotifySessionStarted();
      break;
    case SessionState::kQuerySubmitted:
      NotifyQuerySubmitted();
      break;
    case SessionState::kSessionAbandoned:
      RecordSessionAbandonedMetrics();
      break;
    // On navigation occurrences, keep track of the session state, but do not
    // record any metrics until the end of the session, as multiple queries can
    // be submitted, such as in the case were the AIM page is opened in a new
    // tab and the composebox remains open.
    case SessionState::kNavigationOccurred:
      break;
    default:
      DCHECK(session_state_ != SessionState::kNone);
  }
}

void ComposeboxMetricsRecorder::NotifySessionStarted() {
  session_metrics_->session_elapsed_timer =
      std::make_unique<base::ElapsedTimer>();
}

void ComposeboxMetricsRecorder::NotifyQuerySubmitted() {
  base::TimeDelta time_to_query_submission =
      session_metrics_->session_elapsed_timer->Elapsed();
  session_metrics_->time_to_query_submissions.push_back(
      time_to_query_submission);
}

void ComposeboxMetricsRecorder::RecordSessionAbandonedMetrics() {
  base::TimeDelta session_duration =
      session_metrics_->session_elapsed_timer->Elapsed();
  base::UmaHistogramMediumTimes(
      metric_category_name_ + kComposeboxSessionAbandonedDuration,
      session_duration);
  RecordTotalSessionDuration(session_duration);
}

void ComposeboxMetricsRecorder::RecordSessionCompletedMetrics() {
  base::TimeDelta session_duration =
      session_metrics_->session_elapsed_timer->Elapsed();
  for (const auto time_to_query_submission :
       session_metrics_->time_to_query_submissions) {
    base::UmaHistogramMediumTimes(
        metric_category_name_ + kComposeboxQuerySubmissionTime,
        time_to_query_submission);
    base::UmaHistogramMediumTimes(
        metric_category_name_ + kComposeboxSessionCompletionDuration,
        session_duration);
    RecordTotalSessionDuration(session_duration);
  }
}

void ComposeboxMetricsRecorder::RecordTotalSessionDuration(
    base::TimeDelta session_duration) {
  base::UmaHistogramMediumTimes(
      metric_category_name_ + kComposeboxSessionDuration, session_duration);
}
