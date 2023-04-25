// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_MESSAGE_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_MESSAGE_RECORDER_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"

namespace ash::phonehub {

// To record when a CrosState message is sent and if/when a PhoneStatusSnapShot
// message is received.
class CrosStateMessageRecorder {
 public:
  CrosStateMessageRecorder();
  CrosStateMessageRecorder(const CrosStateMessageRecorder&) = delete;
  CrosStateMessageRecorder& operator=(const CrosStateMessageRecorder&) = delete;
  ~CrosStateMessageRecorder();

  void RecordCrosStateMessageSent();
  void RecordPhoneStatusSnapShotReceived();

 private:
  FRIEND_TEST_ALL_PREFIXES(CrosStateMessageRecorderTest, RecordLatency);

  // The priority queue to keep timestamp when CrosState message is sent.
  base::Time message_sent_timestamp_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_MESSAGE_RECORDER_H_
