// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_SESSION_COMPLETION_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_SESSION_COMPLETION_LOGGER_H_

#include <memory>

#include "chromeos/ash/components/tether/tether_session_completion_logger.h"

namespace ash {

namespace tether {

// Test double for TetherSessionCompletionLogger.
class FakeTetherSessionCompletionLogger : public TetherSessionCompletionLogger {
 public:
  FakeTetherSessionCompletionLogger();

  FakeTetherSessionCompletionLogger(const FakeTetherSessionCompletionLogger&) =
      delete;
  FakeTetherSessionCompletionLogger& operator=(
      const FakeTetherSessionCompletionLogger&) = delete;

  ~FakeTetherSessionCompletionLogger() override;

  TetherSessionCompletionLogger::SessionCompletionReason*
  last_session_completion_reason() {
    return last_session_completion_reason_.get();
  }

  // TetherSessionCompletionLogger:
  void RecordTetherSessionCompletion(
      const SessionCompletionReason& reason) override;

 private:
  std::unique_ptr<TetherSessionCompletionLogger::SessionCompletionReason>
      last_session_completion_reason_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_TETHER_SESSION_COMPLETION_LOGGER_H_
