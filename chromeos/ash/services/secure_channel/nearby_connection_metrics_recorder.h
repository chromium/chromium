// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_METRICS_RECORDER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_METRICS_RECORDER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"

namespace base {
class Time;
}

namespace ash::secure_channel {

// Records the effective success rate for Nearby Connections attempts. In this
// context, "effective" means that (1) a failure followed by a successful retry
// is counted as a success, and (2) repeated failures (e.g., due to users stuck
// in an unrecoverable state due to Bluetooth issues) are only counted as a
// single failure.
//
// To implement this metric, we log every successful attempt as a success. When
// a failure occurs, we wait one minute to see whether a retry succeeds before
// that point. If there was no success in that time frame, we then log a
// failure.
class NearbyConnectionMetricsRecorder {
 public:
  NearbyConnectionMetricsRecorder();
  ~NearbyConnectionMetricsRecorder();

  void HandleConnectionSuccess(const DeviceIdPair& device_id_pair);
  void HandleConnectionFailure(const DeviceIdPair& device_id_pair);

 private:
  void OnTimeout(const DeviceIdPair& device_id_pair);

  // Stores a null Time when the last attempt was successful.
  base::flat_map<DeviceIdPair, base::Time>
      id_pair_to_first_unsuccessful_timestamp_map_;

  base::WeakPtrFactory<NearbyConnectionMetricsRecorder> weak_ptr_factory_{this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_METRICS_RECORDER_H_
