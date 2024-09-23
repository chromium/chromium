// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_UTILS_H_
#define COMPONENTS_INPUT_UTILS_H_

#include "base/component_export.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace input {

COMPONENT_EXPORT(INPUT) bool IsTransferInputToVizSupported();

perfetto::protos::pbzero::ChromeLatencyInfo2::InputType InputEventTypeToProto(
    blink::WebInputEvent::Type event_type);

perfetto::protos::pbzero::ChromeLatencyInfo2::InputResultState
InputEventResultStateToProto(blink::mojom::InputEventResultState result_state);

}  // namespace input

#endif  // COMPONENTS_INPUT_UTILS_H_
