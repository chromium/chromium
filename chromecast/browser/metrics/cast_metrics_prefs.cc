// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/metrics/cast_metrics_prefs.h"

#include "chromecast/metrics/cast_metrics_service_client.h"
#include "components/metrics/metrics_service.h"

namespace chromecast {
namespace metrics {

void RegisterPrefs(PrefRegistrySimple* registry) {
  ::metrics::MetricsService::RegisterPrefs(registry);
  CastMetricsServiceClient::RegisterPrefs(registry);
}

}  // namespace metrics
}  // namespace chromecast
