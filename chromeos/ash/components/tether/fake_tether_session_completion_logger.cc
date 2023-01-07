// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_tether_session_completion_logger.h"

namespace ash {

namespace tether {

FakeTetherSessionCompletionLogger::FakeTetherSessionCompletionLogger() =
    default;

FakeTetherSessionCompletionLogger::~FakeTetherSessionCompletionLogger() =
    default;

void FakeTetherSessionCompletionLogger::RecordTetherSessionCompletion(
    const SessionCompletionReason& reason) {
  last_session_completion_reason_ =
      std::make_unique<SessionCompletionReason>(reason);
}

}  // namespace tether

}  // namespace ash
