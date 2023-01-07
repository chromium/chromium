// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/noisy_metrics_recorder.h"

#include <cmath>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/rand_util.h"

NoisyMetricsRecorder::NoisyMetricsRecorder() = default;
NoisyMetricsRecorder::~NoisyMetricsRecorder() = default;

uint32_t NoisyMetricsRecorder::GetNoisyMetric(float flip_probability,
                                              uint32_t original_metric,
                                              uint8_t count_bits) {
  DCHECK_LE(flip_probability, 1.0f);
  DCHECK_GE(flip_probability, 0.0f);

  // |original_metric| should fit within least significant |count_bits|.
  DCHECK_LE(original_metric, std::pow(2, count_bits) - 1);

  uint32_t flipped_value = 0u;

  for (size_t idx = 0; idx < count_bits; ++idx) {
    uint32_t flipped_bit = 0;
    uint32_t bit = original_metric & 0x1;
    original_metric = original_metric >> 1;
    float first_coin_flip = GetRandBetween0And1();
    if (first_coin_flip < flip_probability) {
      flipped_bit = GetRandEither0Or1();
    } else {
      flipped_bit = bit;
    }
    flipped_value |= (flipped_bit << idx);
  }

  // The method has iterated through least significant |count_bits| and did that
  // many right shifts. At this point, rest of the bits in |original_metric|
  // should be 0.
  DCHECK_EQ(original_metric, 0u);
  DCHECK_EQ(flipped_value >> count_bits, 0u);
  return flipped_value;
}

float NoisyMetricsRecorder::GetRandBetween0And1() const {
  return base::RandDouble();
}

int NoisyMetricsRecorder::GetRandEither0Or1() const {
  return base::RandInt(0, 1);
}