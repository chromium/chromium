// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_NOISY_METRICS_RECORDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_NOISY_METRICS_RECORDER_H_

#include <stdint.h>

// NoisyMetricsRecorder can be used to add noise to metrics before recording
// them.
class NoisyMetricsRecorder {
 public:
  NoisyMetricsRecorder();
  ~NoisyMetricsRecorder();
  // |flip_probability| is the probability that an individual bit will get
  // flipped. Typical value of flip_probability is 0.5. |original_metric| is the
  // metric that contains a value that typically needs at most |count_bits| to
  // be expressed. The method returns the flipped value of |original_metric|.
  uint32_t GetNoisyMetric(float flip_probability,
                          uint32_t original_metric,
                          uint8_t count_bits);

 protected:
  // Returns a random float between 0 and 1 (both inclusive).
  virtual float GetRandBetween0And1() const;

  // Returns a random number that's either 0 or 1.
  virtual int GetRandEither0Or1() const;
};

#endif  //  COMPONENTS_OPTIMIZATION_GUIDE_CORE_NOISY_METRICS_RECORDER_H_