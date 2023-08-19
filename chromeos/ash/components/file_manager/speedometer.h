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
  void SetTotalBytes(int64_t total_bytes);

  // Gets the number of samples currently maintained.
  size_t GetSampleCount() const;

  // Gets the projected remaining time. It can be negative, or TimeDelta::Max()
  // if there aren't enough samples yet.
  base::TimeDelta GetRemainingTime() const;

  // Adds a sample with the current timestamp and the given number of bytes.
  // Does nothing if the previous sample was received less than 3 seconds ago.
  // Returns true if the sample was taken in account.
  // `total_processed_bytes`: Total bytes processed by the task so far.
  bool Update(int64_t bytes);

 private:
  struct Sample {
    // Time when the sample was created.
    base::TimeTicks time;

    // Total bytes processed up to this point in time.
    int64_t bytes;
  };

  // The expected total number of bytes, which will be reached when the task
  // finishes.
  int64_t total_bytes_ = 0;

  // Maintains the 20 most recent samples.
  base::RingBuffer<Sample, 20> samples_;
};

}  // namespace file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_SPEEDOMETER_H_
