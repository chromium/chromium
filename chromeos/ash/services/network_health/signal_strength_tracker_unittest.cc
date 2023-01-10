// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/network_health/signal_strength_tracker.h"

#include <array>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash::network_health {

namespace {

constexpr std::array<uint8_t, 3> kSamples = {71, 74, 80};

}

class SignalStrengthTrackerTest : public ::testing::Test {
 public:
  SignalStrengthTrackerTest() {}

  void SetUp() override {
    tracker_ = std::make_unique<SignalStrengthTracker>();
  }

 protected:
  void PopulateSamples() {
    for (auto s : kSamples) {
      tracker_->AddSample(s);
    }
  }

  std::unique_ptr<SignalStrengthTracker>& tracker() { return tracker_; }

 private:
  std::unique_ptr<SignalStrengthTracker> tracker_;
};

// Verify that the Average of samples is calculated correctly.
TEST_F(SignalStrengthTrackerTest, Average) {
  PopulateSamples();
  ASSERT_FLOAT_EQ(75.0, tracker()->Average());
}

// Verify that the Standard Deviation of samples is calculated correctly.
TEST_F(SignalStrengthTrackerTest, StdDev) {
  PopulateSamples();
  ASSERT_FLOAT_EQ(4.5825757, tracker()->StdDev());
}

// Verify that the samples lists match. The samples list inside of the tracker
// are stored with the most recent value first.
TEST_F(SignalStrengthTrackerTest, Samples) {
  PopulateSamples();
  auto stored_samples = tracker()->Samples();
  auto size = stored_samples.size();
  ASSERT_EQ(kSamples.size(), size);
  for (size_t i = 0; i < kSamples.size(); i++) {
    ASSERT_EQ(kSamples[i], stored_samples[size - i - 1]);
  }
}

// Verify that the size of the samples in the tracker do not exceed the max
// size.
TEST_F(SignalStrengthTrackerTest, SamplesSize) {
  size_t num_samples = kSignalStrengthListSize + 10;
  for (size_t i = 0; i < num_samples; i++) {
    tracker()->AddSample(i);
  }

  ASSERT_EQ(tracker()->Samples().size(), kSignalStrengthListSize);
  ASSERT_EQ(tracker()->Samples()[0], num_samples - 1);
}

}  // namespace ash::network_health
