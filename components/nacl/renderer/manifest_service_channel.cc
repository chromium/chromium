// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/manifest_service_channel.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace nacl {

ManifestServiceChannel::ManifestServiceChannel(
    const IPC::ChannelHandle& handle,
    const base::Callback<void(int32_t)>& connected_callback,
    std::unique_ptr<Delegate> delegate,
    base::WaitableEvent* waitable_event)
    : connected_callback_(connected_callback),
      delegate_(std::move(delegate)),
      channel_(IPC::SyncChannel::Create(
          handle,
          IPC::Channel::MODE_CLIENT,
          this,
          content::RenderThread::Get()->GetIOTaskRunner(),
          base::ThreadTaskRunnerHandle::Get(),
          true,
          waitable_event)),
      peer_pid_(base::kNullProcessId) {}

ManifestServiceChannel::~ManifestServiceChannel() {
  if (!connected_callback_.is_null())
    std::move(connected_callback_).Run(PP_ERROR_FAILED);
}

void ManifestServiceChannel::Send(IPC::Message* message) {
  channel_->Send(message);
}

bool ManifestServiceChannel::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ManifestServiceChannel, message)
      IPC_MESSAGE_HANDLER(PpapiHostMsg_StartupInitializationComplete,
                          OnStartupInitializationComplete)
      IPC_MESSAGE_HANDLER_DELAY_REPLY(PpapiHostMsg_OpenResource,
                                      OnOpenResource)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ManifestServiceChannel::OnChannelConnected(int32_t peer_pid) {
  peer_pid_ = peer_pid;
  if (!connected_callback_.is_null())
    std::move(connected_callback_).Run(PP_OK);
}

void ManifestServiceChannel::OnChannelError() {
  if (!connected_callback_.is_null())
    std::move(connected_callback_).Run(PP_ERROR_FAILED);
}

void ManifestServiceChannel::OnStartupInitializationComplete() {
  delegate_->StartupInitializationComplete();
}

void ManifestServiceChannel::OnOpenResource(
    const std::string& key, IPC::Message* reply) {
  delegate_->OpenResource(
      key,
      base::Bind(&ManifestServiceChannel::DidOpenResource,
                 weak_ptr_factory_.GetWeakPtr(), reply));
}

void ManifestServiceChannel::DidOpenResource(IPC::Message* reply,
                                             base::File file,
                                             uint64_t token_lo,
                                             uint64_t token_hi) {
  ppapi::proxy::SerializedHandle handle;
  if (file.IsValid()) {
    IPC::PlatformFileForTransit file_for_transit =
        IPC::TakePlatformFileForTransit(std::move(file));
    handle.set_file_handle(file_for_transit, PP_FILEOPENFLAG_READ, 0);
  }
  PpapiHostMsg_OpenResource::WriteReplyParams(reply, token_lo, token_hi,
                                              handle);
  Send(reply);
}

}  // namespace nacl
