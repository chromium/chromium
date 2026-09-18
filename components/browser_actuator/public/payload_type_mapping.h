// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_PAYLOAD_TYPE_MAPPING_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_PAYLOAD_TYPE_MAPPING_H_

#include <array>
#include <optional>

#include "base/notreached.h"
#include "components/browser_actuator/public/common.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"

namespace browser_actuator {

// Translation between the `ActuatorDownstreamPayloadType` wire enum and the
// public `PayloadType` enum. This lives in public/ because chrome/browser
// consumers need it and cannot reach into internal/.
//
// `ToDownstreamProtoPayloadType()` is the single source of truth; the reverse
// direction is derived from it by lookup, because a generated proto enum
// cannot be switched over exhaustively.

// Every payload type that can be routed to a handler. `kUnspecified` is
// excluded: it is the proto default and never names a real destination.
// LINT.IfChange(RoutablePayloadTypes)
inline constexpr auto kRoutablePayloadTypes = std::to_array<PayloadType>(
    {PayloadType::kControl, PayloadType::kExperimentalTriggering});

// Returns the wire enum used to tag a downstream payload of `payload_type`.
inline ActuatorDownstreamPayloadType ToDownstreamProtoPayloadType(
    PayloadType payload_type) {
  switch (payload_type) {
    case PayloadType::kControl:
      return ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND;
    case PayloadType::kExperimentalTriggering:
      return ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING;
    case PayloadType::kUnspecified:
      NOTREACHED();
  }
  NOTREACHED();
}
// LINT.ThenChange(//components/browser_actuator/public/common.h:PayloadType)

// Returns the payload type that `proto_type` routes to, or `std::nullopt` if it
// is unspecified or not a value this client knows how to route. Callers must
// treat `std::nullopt` as "drop and report", never as a silent no-op.
inline std::optional<PayloadType> FromDownstreamProtoPayloadType(
    ActuatorDownstreamPayloadType proto_type) {
  if (proto_type == ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED) {
    return std::nullopt;
  }
  for (PayloadType payload_type : kRoutablePayloadTypes) {
    if (ToDownstreamProtoPayloadType(payload_type) == proto_type) {
      return payload_type;
    }
  }
  return std::nullopt;
}

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_PAYLOAD_TYPE_MAPPING_H_
