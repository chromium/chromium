// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_SPEEDOMETER_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_SPEEDOMETER_H_

#include "base/component_export.h"
#include "base/containers/ring_buffer.h"
#include "base/time/time.h"

namespace file_manager {

// Calculates the remaining time for an operation based on the initial total
// bytes and the amount of bytes transferred on each `sample`.
//
// It estimates when the total bytes will be reached and exposes the "remaining
// time" from now until the projected end time.
class COMPONENT_EXPORT(FILE_MANAGER) Speedometer {
 public:
  // Sets the expected total number of bytes for the operation.
  void SetTotalBytes(int64_t total_bytes) { total_bytes_ = total_bytes; }

  // Gets the number of samples currently maintained.
  size_t GetSampleCount() const;

  // Projected remaining time, it can be negative or infinity.
  double GetRemainingSeconds() const;

  // Adds a sample with the current timestamp and the given number of bytes.
  // Does nothing if the previous sample was received less than a second ago.
  // `total_processed_bytes`: Total bytes processed by the task so far.
  void Update(int64_t total_processed_bytes);

 private:
  // Computes a linear interpolation of the samples stored in |samples_|.
  void Interpolate();

  struct Sample {
    // Time when the sample was created, in seconds since `start_time_`.
    double time;

    // Total bytes processed up to this point in time.
    int64_t bytes;
  };

  // Time the Speedometer started. Used to calculate the delta from here to each
  // sample time.
  const base::TimeTicks start_time_ = base::TimeTicks::Now();

  // The expected total number of bytes, which will be reached when the task
  // finishes.
  int64_t total_bytes_ = 0;

  // The projected time to finish the operation, in seconds from `start_time_`.
  double end_time_ = 0;

  // Maintains the 20 most recent samples.
  base::RingBuffer<Sample, 20> samples_;
};

}  // namespace file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_SPEEDOMETER_H_
