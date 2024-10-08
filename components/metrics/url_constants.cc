// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/url_constants.h"

namespace metrics {

// Chrome metrics URLs are stored internally to prevent Chromium forks from
// accidentally sending metrics to Google servers. The URLs can be found here:
// https://chrome-internal.googlesource.com/chrome/components/metrics/internal/
const char kNewMetricsServerUrl[] = "";
const char kNewMetricsServerUrlInsecure[] = "";
const char kOldMetricsServerUrl[] = "";
const char kDefaultMetricsMimeType[] = "";
const char kDefaultUkmServerUrl[] = "";
const char kUkmMimeType[] = "";
const char kDefaultDwaServerUrl[] = "";

} // namespace metrics
