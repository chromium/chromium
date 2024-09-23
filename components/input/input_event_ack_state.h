// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_INPUT_EVENT_ACK_STATE_H_
#define COMPONENTS_INPUT_INPUT_EVENT_ACK_STATE_H_

namespace input {

const char* InputEventResultStateToString(
    blink::mojom::InputEventResultState ack_state);

}  // namespace input

#endif  // COMPONENTS_INPUT_INPUT_EVENT_ACK_STATE_H_
