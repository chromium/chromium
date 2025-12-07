// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_constants.h"

#include "base/containers/map_util.h"
#include "base/strings/strcat.h"

namespace enterprise_connectors {

std::string GetPayloadSizeUmaMetricName(std::string_view event_name) {
  auto* metric_name =
      base::FindOrNull(kEventNameToUmaMetricNameMap, event_name);
  return metric_name ? base::StrCat({*metric_name, "UploadSize"})
                     : base::StrCat({kUnknownUmaMetricName, "UploadSize"});
}

std::string GetPayloadSizeUmaMetricName(EventCase event_case) {
  auto* metric_name =
      base::FindOrNull(kEventCaseToUmaMetricNameMap, event_case);
  return metric_name ? base::StrCat({*metric_name, "UploadSize"})
                     : base::StrCat({kUnknownUmaMetricName, "UploadSize"});
}

std::string GetEventName(EventCase event_case) {
  const std::string_view* event_name =
      base::FindOrNull(kEventCaseToEventNameMap, event_case);

  return event_name ? std::string(*event_name) : std::string("UNKNOWN");
}

}  // namespace enterprise_connectors
