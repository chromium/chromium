// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_EXPERIMENT_METRICS_H_
#define COMPONENTS_CBOR_EXPERIMENT_METRICS_H_

#include <optional>

#include "components/cbor/cbor_buildflags.h"

namespace cbor::internal {

// Returns true if an operation should record UMA metrics.
//
// Both C++ and Rust implementations report to the same histograms, separated
// by Finch experiment arm. Only operations controlled by the feature flag
// (`!use_rust.has_value()`) in Rust-enabled builds are recorded; explicit
// overrides and non-Rust builds opt out to avoid skewing experiment arms.
constexpr bool ShouldRecordMetrics(
    [[maybe_unused]] std::optional<bool> use_rust) {
#if BUILDFLAG(USE_CBOR_RUST)
  return !use_rust.has_value();
#else
  return false;
#endif
}

}  // namespace cbor::internal

#endif  // COMPONENTS_CBOR_EXPERIMENT_METRICS_H_
