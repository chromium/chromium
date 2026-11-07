// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_COMMON_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_COMMON_H_

namespace browser_actuator::transport {

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
//     //components/browser_actuator/internal/transport/metrics_utils.h:PayloadTypeToMetricSuffix,
//     //components/browser_actuator/internal/transport/session_stream_recorder.cc:FromUpstreamProtoPayloadType,
//     //components/browser_actuator/public/transport/payload_type_mapping.h:RoutablePayloadTypes,
//     //tools/metrics/histograms/metadata/browser_actuator/histograms.xml:BrowserActuatorTransportPayloadType)

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
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_COMMON_H_
