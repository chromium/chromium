// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_constants.h"

#include "base/containers/map_util.h"

namespace enterprise_connectors {

std::string_view GetPayloadSizeUmaMetricName(std::string_view eventName) {
  auto* metric_name =
      base::FindOrNull(kEventNameToUmaUploadSizeMetricNameMap, eventName);
  return metric_name ? *metric_name : kUnknownUploadSizeUmaMetricName;
}

std::string_view GetPayloadSizeUmaMetricName(EventCase eventCase) {
  auto* metric_name =
      base::FindOrNull(kEventCaseToUmaUploadSizeMetricNameMap, eventCase);
  return metric_name ? *metric_name : kUnknownUploadSizeUmaMetricName;
}

}  // namespace enterprise_connectors
