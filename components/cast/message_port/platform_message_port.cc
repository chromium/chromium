// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/platform_message_port.h"

#include "build/build_config.h"
#include "components/cast/message_port/message_port_buildflags.h"

#if BUILDFLAG(USE_MESSAGE_PORT_CORE)
#include "components/cast/message_port/cast_core/create_message_port_core.h"  // nogncheck
#elif BUILDFLAG(IS_FUCHSIA)
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"  // nogncheck
#else
#include "components/cast/message_port/cast/message_port_cast.h"  // nogncheck
#endif

namespace cast_api_bindings {

// static
void CreatePlatformMessagePortPair(std::unique_ptr<MessagePort>* client,
                                   std::unique_ptr<MessagePort>* server) {
#if BUILDFLAG(USE_MESSAGE_PORT_CORE)
  return CreateMessagePortCorePair(client, server);
#elif BUILDFLAG(IS_FUCHSIA)
  return MessagePortFuchsia::CreatePair(client, server);
#else
  return MessagePortCast::CreatePair(client, server);
#endif
}

}  // namespace cast_api_bindings
