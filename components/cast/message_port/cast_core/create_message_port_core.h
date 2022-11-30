// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_CREATE_MESSAGE_PORT_CORE_H_
#define COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_CREATE_MESSAGE_PORT_CORE_H_

#include <memory>

#include "components/cast/message_port/cast_core/message_port_core.h"
#include "components/cast/message_port/message_port.h"

namespace cast_api_bindings {

// Creates an MessagePort of the appropriate type based on the current
// TaskRunner. The port is unconnected but assigned to |channel_id|. Typically
// this would be used when receiving a serialized port and converting it to
// the local port type.
std::unique_ptr<MessagePortCore> CreateMessagePortCore(uint32_t channel_id);

// Implementation used by CreatePlatformMessagePortPair
// or callers who want to manually create MessagePortCore.
// This is external to the MessagePortCore class because MessagePortCore does
// not know about all of its implementations.
void CreateMessagePortCorePair(std::unique_ptr<MessagePort>* client,
                               std::unique_ptr<MessagePort>* server);

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_CREATE_MESSAGE_PORT_CORE_H_
