// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_URL_CONSTANTS_H_
#define COMPONENTS_METRICS_URL_CONSTANTS_H_

namespace metrics {

// The new metrics server's URL.
extern const char kNewMetricsServerUrl[];

// The HTTP fallback metrics server's URL.
extern const char kNewMetricsServerUrlInsecure[];

// The old metrics server's URL.
extern const char kOldMetricsServerUrl[];

// The default MIME type for the uploaded metrics data.
extern const char kDefaultMetricsMimeType[];

// The UKM server's URL.
extern const char kDefaultUkmServerUrl[];

// The UKM server's MIME type.
extern const char kUkmMimeType[];

// The default DWA server's URL.
extern const char kDefaultDwaServerUrl[];

} // namespace metrics

#endif // COMPONENTS_METRICS_URL_CONSTANTS_H_
