// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_service.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/scoped_file.h"
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"
#endif

namespace {

mojo::IncomingInvitation GetMojoInvitation() {
  mojo::PlatformChannelEndpoint endpoint;
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(base::ScopedFD(
      base::GlobalDescriptors::GetInstance()->Get(kMojoIPCChannel))));
  DCHECK(endpoint.is_valid());
  return mojo::IncomingInvitation::Accept(std::move(endpoint));
}

}  // namespace

NaClService::NaClService(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : ipc_support_(std::move(ipc_task_runner),
                   mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST) {}

NaClService::~NaClService() = default;

mojo::ScopedMessagePipeHandle NaClService::TakeChannelPipe() {
  return GetMojoInvitation().ExtractMessagePipe(0);
}
