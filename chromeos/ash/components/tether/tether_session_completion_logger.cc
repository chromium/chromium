// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/tether_session_completion_logger.h"

#include "base/metrics/histogram_macros.h"

namespace ash {

namespace tether {

TetherSessionCompletionLogger::TetherSessionCompletionLogger() = default;

TetherSessionCompletionLogger::~TetherSessionCompletionLogger() = default;

void TetherSessionCompletionLogger::RecordTetherSessionCompletion(
    const SessionCompletionReason& reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.SessionCompletionReason", reason,
      SessionCompletionReason::SESSION_COMPLETION_REASON_MAX);
}

}  // namespace tether

}  // namespace ash
