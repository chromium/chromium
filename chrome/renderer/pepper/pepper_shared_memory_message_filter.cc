// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_shared_memory_message_filter.h"

#include <memory>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/process_handle.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var_tracker.h"

PepperSharedMemoryMessageFilter::PepperSharedMemoryMessageFilter(
    content::RendererPpapiHost* host)
    : InstanceMessageFilter(host->GetPpapiHost()), host_(host) {}

PepperSharedMemoryMessageFilter::~PepperSharedMemoryMessageFilter() {}

bool PepperSharedMemoryMessageFilter::OnInstanceMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PepperSharedMemoryMessageFilter, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_SharedMemory_CreateSharedMemory,
                        OnHostMsgCreateSharedMemory)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool PepperSharedMemoryMessageFilter::Send(IPC::Message* msg) {
  return host_->GetPpapiHost()->Send(msg);
}

void PepperSharedMemoryMessageFilter::OnHostMsgCreateSharedMemory(
    PP_Instance instance,
    uint32_t size,
    int* host_handle_id,
    ppapi::proxy::SerializedHandle* plugin_handle) {
  plugin_handle->set_null_shmem_region();
  *host_handle_id = -1;
  base::UnsafeSharedMemoryRegion shm =
      base::UnsafeSharedMemoryRegion::Create(size);
  if (!shm.IsValid())
    return;

  plugin_handle->set_shmem_region(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          host_->ShareUnsafeSharedMemoryRegionWithRemote(shm)));

  *host_handle_id =
      content::PepperPluginInstance::Get(instance)
          ->GetVarTracker()
          ->TrackSharedMemoryRegion(instance, std::move(shm), size);
}
