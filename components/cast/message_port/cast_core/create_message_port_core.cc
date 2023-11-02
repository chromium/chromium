// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/cast_core/create_message_port_core.h"

#include <memory>

#include "components/cast/message_port/cast_core/message_port_core_with_task_runner.h"

namespace cast_api_bindings {

// static
std::unique_ptr<MessagePortCore> CreateMessagePortCore(uint32_t channel_id) {
  return std::make_unique<MessagePortCoreWithTaskRunner>(channel_id);
}

// static
void CreateMessagePortCorePair(std::unique_ptr<MessagePort>* client,
                               std::unique_ptr<MessagePort>* server) {
  auto pair_raw = MessagePortCoreWithTaskRunner::CreatePair();
  *client = std::make_unique<MessagePortCoreWithTaskRunner>(
      std::move(pair_raw.first));
  *server = std::make_unique<MessagePortCoreWithTaskRunner>(
      std::move(pair_raw.second));
}

}  // namespace cast_api_bindings
