// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_MESSAGE_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_MESSAGE_RECORDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"

namespace ash::phonehub {

// To record when a CrosState message is sent and if/when a PhoneStatusSnapShot
// message is received.
class CrosStateMessageRecorder : FeatureStatusProvider::Observer {
 public:
  explicit CrosStateMessageRecorder(
      FeatureStatusProvider* feature_status_provider);
  CrosStateMessageRecorder(const CrosStateMessageRecorder&) = delete;
  CrosStateMessageRecorder& operator=(const CrosStateMessageRecorder&) = delete;
  ~CrosStateMessageRecorder() override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  void RecordCrosStateMessageSent();
  void RecordPhoneStatusSnapShotReceived();

 private:
  FRIEND_TEST_ALL_PREFIXES(CrosStateMessageRecorderTest, RecordLatency);
  FRIEND_TEST_ALL_PREFIXES(CrosStateMessageRecorderTest, RecordSuccess);

  void RecordWhenDisconnected();

  raw_ptr<FeatureStatusProvider, ExperimentalAsh> feature_status_provider_;
  bool is_cros_state_message_sent_ = false;
  bool is_phone_status_snapshot_processed_ = false;
  // The priority queue to keep timestamp when CrosState message is sent.
  base::Time message_sent_timestamp_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_CROS_STATE_MESSAGE_RECORDER_H_
