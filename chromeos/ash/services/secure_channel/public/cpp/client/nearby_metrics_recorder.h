// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_METRICS_RECORDER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_METRICS_RECORDER_H_

#include "base/time/time.h"

namespace ash::secure_channel {

namespace mojom {
enum class ConnectionAttemptFailureReason;
}

// Interface for recording connection metrics.
class NearbyMetricsRecorder {
 public:
  NearbyMetricsRecorder();
  virtual ~NearbyMetricsRecorder();

  // Records connection success and the latency from the start of the
  // connection attempt.
  void RecordConnectionSuccess(const base::TimeDelta latency);

  // Records connection failure and the specific reason.
  void RecordConnectionFailure(mojom::ConnectionAttemptFailureReason reason);

  // Records the length of time that we stayed connected.
  virtual void RecordConnectionDuration(const base::TimeDelta duration) = 0;

 protected:
  virtual void RecordConnectionLatency(const base::TimeDelta latency) = 0;
  virtual void RecordConnectionResult(bool success) = 0;
  virtual void RecordConnectionFailureReason(
      mojom::ConnectionAttemptFailureReason reason) = 0;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_METRICS_RECORDER_H_
