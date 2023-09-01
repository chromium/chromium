// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_EXPIRED_HISTOGRAM_UTIL_H_
#define COMPONENTS_METRICS_EXPIRED_HISTOGRAM_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"

namespace metrics {

// Enables histogram expiry checker if it is enabled by field trial. Histogram
// expiry is disbaled by default so that unit tests don't fail unexpectedly when
// a histogram expires.
void EnableExpiryChecker(base::span<const uint32_t> expired_histograms_hashes);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_EXPIRED_HISTOGRAM_UTIL_H_
