// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/composebox/composebox_query_controller.h"

// TODO(420700441) Add unittest coverage.
void ComposeboxQueryController::NotifySessionStarted() {
  session_start_time_ = base::Time::Now();
}
