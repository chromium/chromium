// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_CLIENT_SIDE_DETECTION_ENUMS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_CLIENT_SIDE_DETECTION_ENUMS_H_

namespace safe_browsing {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ClientSideDetectionEvent)
enum class ClientSideDetectionEvent {
  kTriggerStartsPreClassification = 0,
  kPreClassificationCheckComplete = 1,
  kImageClassificationBegin = 2,
  kImageClassificationComplete = 3,
  kVerdictProtoParseComplete = 4,
  kLocalModelResultComplete = 5,
  kImageEmbeddingBegin = 6,
  kImageEmbeddingComplete = 7,
  kIntelligentScanBegin = 8,
  kIntelligentScanComplete = 9,
  kMiscellaneousFieldsAdded = 10,
  kNetworkRequestSent = 11,
  kNetworkResponseReceived = 12,
  kWarningShown = 13,
  kMaxValue = kWarningShown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:ClientSideDetectionEvent)

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_CLIENT_SIDE_DETECTION_ENUMS_H_
