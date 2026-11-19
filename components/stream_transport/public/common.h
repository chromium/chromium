// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_PUBLIC_COMMON_H_
#define COMPONENTS_STREAM_TRANSPORT_PUBLIC_COMMON_H_

#include "base/check.h"

namespace stream_transport {

// Identifies feature payload targets, matching the server
// DownstreamMessage.PayloadType.
// TODO(crbug.com/532660606): Update this to map to the enum once it is synced
// under components/optimization_guide/proto/.
// LINT.IfChange(PayloadType)
enum class PayloadType {
  kUnspecified = 0,
  kControl = 1,
  kExperimentalTriggering = 2,
  kActuation = 3,
  kTaskIngress = 4,
  kProbe = 5,
};
// LINT.ThenChange(
//     //components/stream_transport/internal/metrics_utils.h:PayloadTypeToMetricSuffix,
//     //components/stream_transport/internal/session_stream_recorder.cc:FromUpstreamProtoPayloadType,
//     //components/stream_transport/public/payload_type_mapping.h:RoutablePayloadTypes,
//     //tools/metrics/histograms/metadata/stream_transport/histograms.xml:StreamTransportPayloadType
// )

// Identifies the feature factory instance, to ensure we do not re-register
// factories of the same type.
enum class FactoryId {
  kUnset = 0,
  kControl = 1,
  kExperimentalTriggering = 2,
  kSessionStreamRecorder = 3,
  kActuation = 4,
  kTaskIngress = 5,
  kProbe = 6,
};

enum class SendUpstreamMessageError {
  kChannelDisconnected,
  kBufferFull,
};

// Describes a failure encountered when starting a client-initiated session.
enum class StartClientSessionError {
  // The transport channel is disabled, unavailable, or lacks auth/capacity.
  kChannelUnavailable,
  // Local concurrent session capacity was reached or the session ID is invalid.
  kSessionLimitReached,
  // The server rejected the session ID because it already exists. Callers may
  // retry with a fresh session ID.
  kSessionIdConflict,
  // A retryable transport or server failure occurred.
  kTransientError,
  // A non-retryable server rejection occurred.
  kPermanentError,
};

// Maps a non-2xx HTTP status code from an upstream session-start request (or a
// non-positive code when no HTTP response was received) to
// `StartClientSessionError`. Callers must only pass non-2xx status codes
// (`http_status_code < 200 || http_status_code >= 300`).
inline StartClientSessionError StartClientSessionErrorFromHttpStatus(
    int http_status_code) {
  CHECK(http_status_code < 200 || http_status_code >= 300);
  if (http_status_code <= 0) {
    return StartClientSessionError::kTransientError;
  }
  if (http_status_code == 409) {
    return StartClientSessionError::kSessionIdConflict;
  }
  if (http_status_code == 429 ||
      (http_status_code >= 500 && http_status_code < 600)) {
    return StartClientSessionError::kTransientError;
  }
  return StartClientSessionError::kPermanentError;
}

// Identifies the downstream ingress channel that initialized a transport
// session.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(InitIngressSource)
enum class InitIngressSource {
  kUnspecified = 0,
  kFcmPush = 1,
  kWatchSessionsStream = 2,
  kLocalChromeIngest = 3,
  kMaxValue = kLocalChromeIngest,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/stream_transport/enums.xml:StreamTransportSessionInitIngressSource)

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_PUBLIC_COMMON_H_
