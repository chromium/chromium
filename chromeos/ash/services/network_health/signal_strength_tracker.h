// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_SIGNAL_STRENGTH_TRACKER_H_
#define CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_SIGNAL_STRENGTH_TRACKER_H_

#include <cstdint>
#include <vector>

#include "base/containers/circular_deque.h"
#include "chromeos/ash/services/network_health/network_health_constants.h"

namespace ash::network_health {

// The size is the list to use for the signal strength calculations. This is
// the sample window multipied by the sample rate.
constexpr uint16_t kSignalStrengthListSize =
    kSignalStrengthSampleWindow.InMinutes() *
    (60 / kSignalStrengthSampleRate.InSeconds());

// SignalStrengthTracker maintains a list of signal strength samples for a
// particular network. The sample list functions as a sliding window with a
// fixed size of kSignalStrengthListSize;
class SignalStrengthTracker {
 public:
  SignalStrengthTracker();

  ~SignalStrengthTracker();

  // Adds a new signal strength sample. If the sample list is already over the
  // maximum size, the oldest value will be removed.
  void AddSample(uint8_t sample);

  // Returns the average signal strength value of the recorded samples.
  float Average();

  // Returns the standard deviation of the recorded signal strength values.
  float StdDev();

  // Returns the raw samples of the signal strength values.
  std::vector<uint8_t> Samples();

 private:
  base::circular_deque<uint8_t> samples_;
  uint16_t count_ = 0;
  uint16_t sum_ = 0;
};

}  // namespace ash::network_health

#endif  // CHROMEOS_ASH_SERVICES_NETWORK_HEALTH_SIGNAL_STRENGTH_TRACKER_H_
