// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/cros_state_message_recorder.h"

#include "base/metrics/histogram_functions.h"

namespace ash::phonehub {

CrosStateMessageRecorder::CrosStateMessageRecorder() = default;
CrosStateMessageRecorder::~CrosStateMessageRecorder() = default;

void CrosStateMessageRecorder::RecordCrosStateMessageSent() {
  // Update the timestamp for CrosState message sent.
  message_sent_timestamp_ = base::Time::NowFromSystemTime();
}

void CrosStateMessageRecorder::RecordPhoneStatusSnapShotReceived() {
  base::UmaHistogramLongTimes(
      "PhoneHub.InitialPhoneStatusSnapshot.Latency",
      base::Time::NowFromSystemTime() - message_sent_timestamp_);
  message_sent_timestamp_ = base::Time();
}

}  // namespace ash::phonehub
