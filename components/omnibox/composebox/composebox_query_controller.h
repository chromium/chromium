// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_

#include "base/time/time.h"

enum class SessionState {
  kNone = 0,
  kSessionStarted = 1,
  kSessionAbandoned = 2,
  kSubmittedQuery = 3,
};

class ComposeboxQueryController {
 public:
  ComposeboxQueryController() = default;
  virtual ~ComposeboxQueryController();

  // Session management. Virtual for testing.
  virtual void NotifySessionStarted();
  virtual void NotifySessionAbandoned();

  SessionState session_state() { return session_state_; }

 private:
  // TODO(420701010) Create SessionMetrics struct.
  base::Time session_start_time_;
  SessionState session_state_ = SessionState::kNone;
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_
