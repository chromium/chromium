// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PREFS_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PREFS_H_

namespace metrics::structured::prefs {

// Preference which stores serialized StructuredMetrics logs to be uploaded.
extern const char kLogStoreName[];

// Preference which stores device keys for Structured Metrics. Currently only
// used on desktop Chrome platforms.
extern const char kDeviceKeyDataPrefName[];

}  // namespace metrics::structured::prefs

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PREFS_H_
