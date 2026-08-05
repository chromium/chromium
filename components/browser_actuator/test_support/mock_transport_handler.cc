// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/test_support/mock_transport_handler.h"

#include <utility>

namespace browser_actuator {

MockTransportHandler::MockTransportHandler() = default;
MockTransportHandler::~MockTransportHandler() = default;

CallbackTransportHandler::CallbackTransportHandler(
    base::OnceClosure on_message_cb)
    : on_message_cb_(std::move(on_message_cb)) {}

CallbackTransportHandler::~CallbackTransportHandler() = default;

void CallbackTransportHandler::OnMessage(std::string_view payload) {
  if (on_message_cb_) {
    base::OnceClosure cb = std::move(on_message_cb_);
    // The callback may synchronously delete `this`.
    // Do not add any code or access members below this call.
    std::move(cb).Run();
  }
}

}  // namespace browser_actuator
