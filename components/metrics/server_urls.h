// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SERVER_URLS_H_
#define COMPONENTS_METRICS_SERVER_URLS_H_

#include "url/gurl.h"

namespace metrics {

// The MIME type for the uploaded metrics data.
extern const char kMetricsMimeType[];

// The UKM server's MIME type.
extern const char kUkmMimeType[];

// The metrics server's URL.
GURL GetMetricsServerUrl();

// The HTTP fallback metrics server's URL.
GURL GetInsecureMetricsServerUrl();

// The metrics server's URL for Cast.
GURL GetCastMetricsServerUrl();

// The UKM server's URL.
GURL GetUkmServerUrl();

// The DWA server's URL.
GURL GetDwaServerUrl();

// The Private Metrics server's URL.
GURL GetPrivateMetricsServerUrl();

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SERVER_URLS_H_
