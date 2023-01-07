// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_SESSION_COMPLETION_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_SESSION_COMPLETION_LOGGER_H_

namespace ash {

namespace tether {

// Wrapper around metrics reporting for how a Tether session ended.
class TetherSessionCompletionLogger {
 public:
  enum SessionCompletionReason {
    OTHER = 0,
    USER_DISCONNECTED = 1,
    CONNECTION_DROPPED = 2,
    USER_LOGGED_OUT = 3,
    USER_CLOSED_LID = 4,
    PREF_DISABLED = 5,
    BLUETOOTH_DISABLED = 6,
    CELLULAR_DISABLED = 7,
    WIFI_DISABLED = 8,
    BLUETOOTH_CONTROLLER_DISAPPEARED = 9,
    MULTIDEVICE_HOST_UNVERIFIED = 10,
    BETTER_TOGETHER_SUITE_DISABLED = 11,
    SESSION_COMPLETION_REASON_MAX
  };

  TetherSessionCompletionLogger();

  TetherSessionCompletionLogger(const TetherSessionCompletionLogger&) = delete;
  TetherSessionCompletionLogger& operator=(
      const TetherSessionCompletionLogger&) = delete;

  virtual ~TetherSessionCompletionLogger();

  virtual void RecordTetherSessionCompletion(
      const SessionCompletionReason& reason);

 private:
  friend class TetherSessionCompletionLoggerTest;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_SESSION_COMPLETION_LOGGER_H_
