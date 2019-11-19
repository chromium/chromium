// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_service.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/embedder/switches.h"

#if defined(OS_POSIX)
#include "base/files/scoped_file.h"
#include "base/posix/global_descriptors.h"
#include "services/service_manager/embedder/descriptors.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/mach_port_rendezvous.h"
#endif

namespace {

mojo::IncomingInvitation GetMojoInvitation() {
  mojo::PlatformChannelEndpoint endpoint;
#if defined(OS_WIN)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#elif defined(OS_MACOSX)
  auto* client = base::MachPortRendezvousClient::GetInstance();
  if (client) {
    endpoint = mojo::PlatformChannelEndpoint(
        mojo::PlatformHandle(client->TakeReceiveRight('mojo')));
  }
#else
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(
          service_manager::kMojoIPCChannel))));
#endif  // !defined(OS_WIN)
  DCHECK(endpoint.is_valid());
  return mojo::IncomingInvitation::Accept(std::move(endpoint));
}

}  // namespace

NaClService::NaClService(
    scoped_refptr<base::SequencedTaskRunner> ipc_task_runner)
    : ipc_support_(std::move(ipc_task_runner),
                   mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST) {}

NaClService::~NaClService() = default;

mojo::ScopedMessagePipeHandle NaClService::TakeChannelPipe() {
  return GetMojoInvitation().ExtractMessagePipe(0);
}
