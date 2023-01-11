// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pnacl_translation_resource_host.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_globals.h"

using ppapi::PpapiGlobals;

PnaclTranslationResourceHost::PnaclTranslationResourceHost(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner), sender_(nullptr) {}

PnaclTranslationResourceHost::~PnaclTranslationResourceHost() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  CleanupCacheRequests();
}

void PnaclTranslationResourceHost::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = channel;
}

void PnaclTranslationResourceHost::OnFilterRemoved() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = nullptr;
}

void PnaclTranslationResourceHost::OnChannelClosing() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  sender_ = nullptr;
}

bool PnaclTranslationResourceHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PnaclTranslationResourceHost, message)
    IPC_MESSAGE_HANDLER(NaClViewMsg_NexeTempFileReply, OnNexeTempFileReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PnaclTranslationResourceHost::RequestNexeFd(
    PP_Instance instance,
    const nacl::PnaclCacheInfo& cache_info,
    RequestNexeFdCallback callback) {
  DCHECK(PpapiGlobals::Get()->
         GetMainThreadMessageLoop()->BelongsToCurrentThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PnaclTranslationResourceHost::SendRequestNexeFd, this,
                     instance, cache_info, std::move(callback)));
  return;
}

void PnaclTranslationResourceHost::SendRequestNexeFd(
    PP_Instance instance,
    const nacl::PnaclCacheInfo& cache_info,
    RequestNexeFdCallback callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!sender_ || !sender_->Send(new NaClHostMsg_NexeTempFileRequest(
                      instance, cache_info))) {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  static_cast<int32_t>(PP_ERROR_FAILED), false,
                                  PP_kInvalidFileHandle));
    return;
  }
  pending_cache_requests_.insert(std::make_pair(instance, std::move(callback)));
}

void PnaclTranslationResourceHost::ReportTranslationFinished(
    PP_Instance instance,
    PP_Bool success) {
  DCHECK(PpapiGlobals::Get()->
         GetMainThreadMessageLoop()->BelongsToCurrentThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PnaclTranslationResourceHost::SendReportTranslationFinished, this,
          instance, success));
  return;
}

void PnaclTranslationResourceHost::SendReportTranslationFinished(
    PP_Instance instance,
    PP_Bool success) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // If the sender is closed or we have been detached, we are probably shutting
  // down, so just don't send anything.
  if (!sender_)
    return;
  DCHECK(pending_cache_requests_.count(instance) == 0);
  sender_->Send(new NaClHostMsg_ReportTranslationFinished(instance,
                                                          PP_ToBool(success)));
}

void PnaclTranslationResourceHost::OnNexeTempFileReply(
    PP_Instance instance,
    bool is_hit,
    IPC::PlatformFileForTransit file) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::File base_file = IPC::PlatformFileForTransitToFile(file);
  auto it = pending_cache_requests_.find(instance);
  if (!base_file.IsValid()) {
    DLOG(ERROR) << "Got invalid platformfilefortransit";
  }
  if (it != pending_cache_requests_.end()) {
    PP_FileHandle file_handle = PP_kInvalidFileHandle;
    int32_t status = PP_ERROR_FAILED;
    if (base_file.IsValid()) {
      file_handle = base_file.TakePlatformFile();
      status = PP_OK;
    }
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(it->second), status, is_hit, file_handle));
    pending_cache_requests_.erase(it);
  } else {
    DLOG(ERROR) << "Could not find pending request for reply";
  }
}

void PnaclTranslationResourceHost::CleanupCacheRequests() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (auto it = pending_cache_requests_.begin();
       it != pending_cache_requests_.end(); ++it) {
    PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(std::move(it->second),
                                  static_cast<int32_t>(PP_ERROR_ABORTED), false,
                                  PP_kInvalidFileHandle));
  }
  pending_cache_requests_.clear();
}
