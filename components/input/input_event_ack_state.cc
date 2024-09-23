// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

#include "base/logging.h"

namespace input {

const char* InputEventResultStateToString(
    blink::mojom::InputEventResultState ack_state) {
  switch (ack_state) {
    case blink::mojom::InputEventResultState::kUnknown:
      return "UNKNOWN";
    case blink::mojom::InputEventResultState::kConsumed:
      return "CONSUMED";
    case blink::mojom::InputEventResultState::kNotConsumed:
      return "NOT_CONSUMED";
    case blink::mojom::InputEventResultState::kNoConsumerExists:
      return "NO_CONSUMER_EXISTS";
    case blink::mojom::InputEventResultState::kIgnored:
      return "IGNORED";
    case blink::mojom::InputEventResultState::kSetNonBlocking:
      return "SET_NON_BLOCKING";
    case blink::mojom::InputEventResultState::kSetNonBlockingDueToFling:
      return "SET_NON_BLOCKING_DUE_TO_FLING";
  }
  DLOG(WARNING)
      << "InputEventResultStateToString: Unhandled InputEventResultState.";
  return "";
}

}  // namespace input
