// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SRC_REQUEST_STATUS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SRC_REQUEST_STATUS_H_

namespace attribution_reporting {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AttributionSrcRequestStatus)
enum class AttributionSrcRequestStatus {
  kRequested = 0,
  kReceived = 1,
  kFailed = 2,
  kRedirected = 3,
  kReceivedAfterRedirected = 4,
  kFailedAfterRedirected = 5,
  kDropped = 6,  // only recorded for in-browser metrics
  kMaxValue = kDropped,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionAttributionSrcRequestStatus)

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_ATTRIBUTION_SRC_REQUEST_STATUS_H_
