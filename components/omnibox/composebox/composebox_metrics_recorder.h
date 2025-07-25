// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_METRICS_RECORDER_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_METRICS_RECORDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"

enum class SessionState {
  kNone = 0,
  kSessionStarted = 1,
  kSessionAbandoned = 2,
  kQuerySubmitted = 3,
  kNavigationOccurred = 4,
};

struct SessionMetrics {
  SessionMetrics();
  ~SessionMetrics();
  // Timer to keep track of the session durations.
  std::unique_ptr<base::ElapsedTimer> session_elapsed_timer;
  // `time_to_query_submissions` will only hold one value, except in the case
  // where a user navigates to the AIM page on a new window or tab and the
  // composebox remains open. In that edge case the user can continue to submit
  // queries, leading to multiple completion sessions recorded.
  std::vector<base::TimeDelta> time_to_query_submissions;
};

// TODO(429010262): Observe ComposeboxQueryController::FileObserver
class ComposeboxMetricsRecorder {
 public:
  explicit ComposeboxMetricsRecorder(std::string metric_component_name);
  virtual ~ComposeboxMetricsRecorder();

  // Should be called when there are session state changes to keep track of
  // session state metrics. Virtual for testing.
  virtual void NotifySessionStateChanged(SessionState session_state);

 private:
  // Called when the session starts to correctly track session
  // durations.
  void NotifySessionStarted();
  // Called when a query is submitted to correctly track the time from
  // the session starting to query submission.
  void NotifyQuerySubmitted();
  // Should only be called when a session has been abandoned.
  void RecordSessionAbandonedMetrics();
  // Should only be called if a query was submitted and navigation to the AIM
  // page occurred.
  void RecordSessionCompletedMetrics();
  // Records session durations regardless of whether the session was abandoned
  // or completed successfully.
  void RecordTotalSessionDuration(base::TimeDelta session_duration);
  std::string metric_category_name_;
  std::unique_ptr<SessionMetrics> session_metrics_;
  SessionState session_state_ = SessionState::kNone;
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_METRICS_RECORDER_H_
