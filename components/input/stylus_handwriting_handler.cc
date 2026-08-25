// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/stylus_handwriting_handler.h"

#include "components/input/input_router_client.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"

namespace input {

StylusHandwritingHandler::StylusHandwritingHandler(InputRouterClient* client)
    : client_(client) {}

StylusHandwritingHandler::~StylusHandwritingHandler() = default;

void StylusHandwritingHandler::OnTouchEvent(const blink::WebTouchEvent&) {}

void StylusHandwritingHandler::ApplyTouchAction(cc::TouchAction) {}

StylusHandwritingHandler::GestureHandlingResult
StylusHandwritingHandler::HandleGesture(const blink::WebGestureEvent&,
                                        std::optional<cc::TouchAction>) {
  return GestureHandlingResult::kNotHandled;
}

void StylusHandwritingHandler::EndStylusWriting() {
  stylus_writing_started_ = false;
}

bool StylusHandwritingHandler::StartStylusWriting() {
  auto* stylus_interface = client_->GetStylusInterface();
  if (!stylus_interface || !stylus_interface->ShouldInitiateStylusWriting()) {
    return false;
  }
  stylus_writing_started_ = true;
  client_->OnStartStylusWriting();
  return true;
}

}  // namespace input
