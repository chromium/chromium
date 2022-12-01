// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_METRICS_RECORDER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_METRICS_RECORDER_H_

#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/nearby_metrics_recorder.h"

namespace ash::phonehub {

class PhoneHubMetricsRecorder : public secure_channel::NearbyMetricsRecorder {
 public:
  PhoneHubMetricsRecorder();
  ~PhoneHubMetricsRecorder() override;

  PhoneHubMetricsRecorder(const PhoneHubMetricsRecorder&) = delete;
  PhoneHubMetricsRecorder& operator=(const PhoneHubMetricsRecorder&) = delete;

  // secure_channel::NearbyMetricsRecorder:
  void RecordConnectionResult(bool success) override;
  void RecordConnectionFailureReason(
      secure_channel::mojom::ConnectionAttemptFailureReason reason) override;
  void RecordConnectionLatency(const base::TimeDelta latency) override;
  void RecordConnectionDuration(const base::TimeDelta duration) override;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_METRICS_RECORDER_H_
