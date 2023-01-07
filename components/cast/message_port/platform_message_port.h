// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_PLATFORM_MESSAGE_PORT_H_
#define COMPONENTS_CAST_MESSAGE_PORT_PLATFORM_MESSAGE_PORT_H_

#include <memory>

#include "components/cast/message_port/message_port.h"

namespace cast_api_bindings {

// Creates a pair of message ports. Clients must respect |client| and
// |server| semantics because some platforms have asymmetric port
// implementations.
void CreatePlatformMessagePortPair(std::unique_ptr<MessagePort>* client,
                                   std::unique_ptr<MessagePort>* server);

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_MESSAGE_PORT_PLATFORM_MESSAGE_PORT_H_
