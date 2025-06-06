// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_log_message_manager.h"

#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"

namespace viz {

namespace {
bool PreInitializeLogHandler(int severity,
                             const char* file,
                             int line,
                             size_t message_start,
                             const std::string& message) {
  GpuLogMessageManager::GetInstance()->AddDeferredMessage(
      severity, message.substr(0, message_start),
      message.substr(message_start));
  return false;
}

bool PostInitializeLogHandler(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& message) {
  GpuLogMessageManager::GetInstance()->RouteMessage(
      severity, message.substr(0, message_start),
      message.substr(message_start));
  return false;
}
}  // namespace

GpuLogMessageManager* GpuLogMessageManager::GetInstance() {
  static base::NoDestructor<GpuLogMessageManager> message_manager;
  return message_manager.get();
}

GpuLogMessageManager::GpuLogMessageManager() = default;

void GpuLogMessageManager::AddDeferredMessage(int severity,
                                              const std::string& header,
                                              const std::string& message) {
  base::AutoLock lock(message_lock_);
  deferred_messages_.emplace_back(severity, header, message);
}

void GpuLogMessageManager::RouteMessage(int severity,
                                        const std::string& header,
                                        const std::string& message) {
  // This can be run from any thread, but mojo messages are sent on the IO
  // thread.
  if (io_task_runner_->BelongsToCurrentThread()) {
    if (!gpu_logging_.is_bound()) {
      // |InstallPostInitializeLogHandler| set a new log message handler, which
      // will call |RouteMessage|. When |RouteMessage| handling log message
      // GPULogging may not yet be bound. Cache those log messages until the
      // Remote is bound.
      base::AutoLock lock(message_lock_);
      deferred_messages_.emplace_back(severity, header, message);
      return;
    }

    gpu_logging_->RecordLogMessage(severity, header, message);
  } else {
    // Unretained is safe because |this| is valid until the process exits.
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuLogMessageManager::RouteMessage,
                                  base::Unretained(this), severity,
                                  std::move(header), std::move(message)));
  }
}

void GpuLogMessageManager::FlushMessages(mojom::GpuLogging* gpu_logging) {
  base::AutoLock lock(message_lock_);
  for (auto& log : deferred_messages_) {
    gpu_logging->RecordLogMessage(log.severity, std::move(log.header),
                                  std::move(log.message));
  }
  deferred_messages_.clear();
}

void GpuLogMessageManager::InstallPreInitializeLogHandler() {
  logging::SetLogMessageHandler(PreInitializeLogHandler);
}

void GpuLogMessageManager::InstallPostInitializeLogHandler(
    mojo::PendingRemote<mojom::GpuLogging> pending_remote,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  Bind(std::move(pending_remote), std::move(io_task_runner));

  logging::SetLogMessageHandler(PostInitializeLogHandler);
}

void GpuLogMessageManager::ShutdownLogging() {
  logging::SetLogMessageHandler(nullptr);

  // |io_task_runner_| may be null if GPULogMessageManager hasn't been bound.
  // Destroy the remote on the IO thread.
  // base::Unretained is safe because this is a singleton.
  if (io_task_runner_) {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuLogMessageManager::ResetLoggingOnIOThread,
                                  base::Unretained(this)));
  }
}

void GpuLogMessageManager::Bind(
    mojo::PendingRemote<mojom::GpuLogging> pending_remote,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  DCHECK(!io_task_runner->BelongsToCurrentThread());
  io_task_runner_ = std::move(io_task_runner);

  // base::Unretained is safe because this is a singleton.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuLogMessageManager::BindOnIOThread,
                     base::Unretained(this), std::move(pending_remote)));
}

void GpuLogMessageManager::BindOnIOThread(
    mojo::PendingRemote<mojom::GpuLogging> pending_remote) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(!gpu_logging_.is_bound());
  gpu_logging_.Bind(std::move(pending_remote));

  FlushMessages(gpu_logging_.get());
}

void GpuLogMessageManager::ResetLoggingOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  gpu_logging_.reset();
}

void GpuLogMessageManager::TerminateProcess(int exit_code) {
  // Block the calling thread so it doesn't execute any more code before the
  // process exits. This function cannot be called from the IO thread.
  CHECK(!io_task_runner_->BelongsToCurrentThread());
  base::WaitableEvent wait;

  // base::Unretained is safe because this is a singleton.
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuLogMessageManager::TerminateProcessOnIO,
                                base::Unretained(this), exit_code));

  wait.Wait();
}

void GpuLogMessageManager::TerminateProcessOnIO(int exit_code) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::Process::TerminateCurrentProcessImmediately(exit_code);
}

}  // namespace viz
