// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/network_health/signal_strength_tracker.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "base/logging.h"

namespace ash::network_health {

SignalStrengthTracker::SignalStrengthTracker() = default;

SignalStrengthTracker::~SignalStrengthTracker() = default;

void SignalStrengthTracker::AddSample(uint8_t sample) {
  if (count_ < kSignalStrengthListSize) {
    count_++;
  } else {
    sum_ -= samples_.back();
    samples_.pop_back();
  }

  samples_.push_front(sample);
  sum_ += sample;
}

float SignalStrengthTracker::Average() {
  if (count_ == 0)
    return 0;
  return static_cast<float>(sum_) / count_;
}

float SignalStrengthTracker::StdDev() {
  if (count_ <= 1)
    return 0.0;

  auto squared_variance_sum = 0;
  for (auto s : samples_) {
    squared_variance_sum += pow(Average() - s, 2);
  }

  return sqrt(squared_variance_sum / (count_ - 1));
}

std::vector<uint8_t> SignalStrengthTracker::Samples() {
  return std::vector<uint8_t>(samples_.begin(), samples_.end());
}

}  // namespace ash::network_health
