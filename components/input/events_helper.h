// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_EVENTS_HELPER_H_
#define COMPONENTS_INPUT_EVENTS_HELPER_H_

#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace input {

// Utility to map the event ack state from the renderer, returns true if the
// event could be handled blocking.
COMPONENT_EXPORT(INPUT)
bool InputEventResultStateIsSetBlocking(blink::mojom::InputEventResultState);

}  // namespace input

#endif  // COMPONENTS_INPUT_EVENTS_HELPER_H_
