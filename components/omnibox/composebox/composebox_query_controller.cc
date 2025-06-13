// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_query_controller.h"

ComposeboxQueryController::~ComposeboxQueryController() {
  // Ensure NTP exits are tracked. i.e. The user starts a composebox session,
  // and closes the NTP without explicitly exiting the session or submitting a
  // query.
  // TODO(420701010): Add unittest coverage, e.g. ensuring abandoned metrics
  // are correctly emitted.
  if (session_state() == SessionState::kSessionStarted) {
    NotifySessionAbandoned();
  }
}

void ComposeboxQueryController::NotifySessionStarted() {
  session_state_ = SessionState::kSessionStarted;
  session_start_time_ = base::Time::Now();
}

void ComposeboxQueryController::NotifySessionAbandoned() {
  session_state_ = SessionState::kSessionAbandoned;
}
