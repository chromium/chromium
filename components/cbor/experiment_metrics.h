// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_EXPERIMENT_METRICS_H_
#define COMPONENTS_CBOR_EXPERIMENT_METRICS_H_

#include <optional>

namespace cbor::internal {

// Returns true if an operation should record UMA metrics.
//
// Both C++ and Rust implementations report to the same histograms, separated
// by Finch experiment arm. Only operations controlled by the feature flag
// (`!use_rust.has_value()`) are recorded; explicit overrides opt out to avoid
// skewing experiment arms.
constexpr bool ShouldRecordMetrics(std::optional<bool> use_rust) {
  return !use_rust.has_value();
}

}  // namespace cbor::internal

#endif  // COMPONENTS_CBOR_EXPERIMENT_METRICS_H_
