// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_

#include "base/time/time.h"

class ComposeboxQueryController {
 public:
  ComposeboxQueryController() = default;
  virtual ~ComposeboxQueryController() = default;

  // Session management.
  void NotifySessionStarted();

 private:
  // TODO(420701010) Create SessionMetrics struct.
  base::Time session_start_time_;
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_COMPOSEBOX_QUERY_CONTROLLER_H_
