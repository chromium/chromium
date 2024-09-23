// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"

#include "base/notreached.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"

#if BUILDFLAG(IS_MAC)
#include <mach/kern_return.h>
#include <mach/mach.h>
#include <mach/message.h>

#include "base/apple/mach_logging.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#endif

namespace named_mojo_ipc_server {

// static
mojo::PlatformChannelEndpoint ConnectToServer(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  return mojo::NamedPlatformChannel::ConnectToServer(server_name);
#elif BUILDFLAG(IS_MAC)
  mojo::PlatformChannelEndpoint endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(server_name);

  mojo::PlatformChannel channel;
  mach_msg_base_t message{};
  base::ScopedMachMsgDestroy scoped_message(&message.header);
  message.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MOVE_SEND);
  message.header.msgh_size = sizeof(message);
  message.header.msgh_local_port =
      channel.TakeLocalEndpoint().TakePlatformHandle().ReleaseMachSendRight();
  message.header.msgh_remote_port =
      endpoint.TakePlatformHandle().ReleaseMachSendRight();

  kern_return_t kr = mach_msg_send(&message.header);
  if (kr == KERN_SUCCESS) {
    scoped_message.Disarm();
  } else {
    MACH_VLOG(1, kr) << "mach_msg_send";
    return mojo::PlatformChannelEndpoint();
  }

  return channel.TakeRemoteEndpoint();
#else
  NOTREACHED_IN_MIGRATION() << "Unsupported platform.";
  return mojo::PlatformChannelEndpoint();
#endif
}

}  // namespace named_mojo_ipc_server
