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

// This enum is used to track the result of the allowlists we use before we
// decide to classify. Currently, only the CSD match can halt classification
// from going forward. These values are persisted to logs. Entries should not
// be renumbered and numeric values should never be reused.
// LINT.IfChange(ClientSideAllowlistMatchResult)
enum class ClientSideAllowlistMatchResult {
  kNoMatch = 0,
  kCsdMatch = 1,
  kHighConfidenceMatch = 2,
  kDeprecatedCsdAndHighConfidenceMatch = 3,  // Deprecated
  kMaxValue = kDeprecatedCsdAndHighConfidenceMatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:ClientSideAllowlistMatchResult)

// Tracks whether a FORCE_REQUEST ping sent by ClientSideDetectionHostBase is
// the immediate first ping or the delayed second ping. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
// LINT.IfChange(ForceRequestPingOrder)
enum class ForceRequestPingOrder {
  kFirstPing = 0,
  kSecondPing = 1,
  kMaxValue = kSecondPing,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientPhishingForceRequestPingOrder)

// Tracks the lifecycle of the delayed second FORCE_REQUEST ping when
// kClientSideDetectionSecondForceRequestWithDelay is enabled. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
// LINT.IfChange(DelayedForceRequestPingEvent)
enum class DelayedForceRequestPingEvent {
  kScheduled = 0,
  kStarted = 1,
  kCancelled = 2,
  kSent = 3,
  kSkippedUrlChanged = 4,
  kMaxValue = kSkippedUrlChanged,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientPhishingDelayedForceRequestPingEvent)

// Tracks the outcome of capturing a viewport screenshot for multimodal
// intelligent scan when `ShouldCaptureScreenshotForIntelligentScan()` is true.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(MultimodalScreenshotCaptureStatus)
enum class MultimodalScreenshotCaptureStatus {
  kSuccess = 0,
  kTabNotVisibleOrUrlChanged = 1,
  kRenderWidgetHostViewUnavailable = 2,
  kCopyFromSurfaceFailed = 3,
  kEmptyBitmap = 4,
  kEncodingFailed = 5,
  kWebStateUnavailable = 6,
  kPageContextFailed = 7,
  kSnapshotMissing = 8,
  kBase64DecodeFailed = 9,
  kMaxValue = kBase64DecodeFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientPhishingMultimodalScreenshotCaptureStatus)

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_CLIENT_SIDE_DETECTION_ENUMS_H_
