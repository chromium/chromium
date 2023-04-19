// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/url_constants.h"

namespace metrics {

const char kNewMetricsServerUrl[] =
    "https://clientservices.googleapis.com/uma/v2";

const char kNewMetricsServerUrlInsecure[] =
    "http://clientservices.googleapis.com/uma/v2";

const char kOldMetricsServerUrl[] = "https://clients4.google.com/uma/v2";

const char kDefaultMetricsMimeType[] = "application/vnd.chrome.uma";

const char kDefaultUkmServerUrl[] = "https://clients4.google.com/ukm";

const char kUkmMimeType[] = "application/vnd.chrome.ukm";

} // namespace metrics
