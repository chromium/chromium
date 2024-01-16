// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/structured/structured_metrics_utils.h"

#include "base/i18n/number_formatting.h"
#include "components/metrics/structured/structured_metrics_service.h"

namespace metrics::structured {

base::Value GetStructuredMetricsSummary(StructuredMetricsService* service) {
  base::Value::Dict result;
  result.Set("enabled", service->recording_enabled());
  auto id =
      service->recorder()->key_data_provider()->GetSecondaryId("CrOSEvents");
  if (id.has_value()) {
    result.Set("crosDeviceId", base::NumberToString(id.value()));
  }
  return base::Value(std::move(result));
}

}  // namespace metrics::structured
