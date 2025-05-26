// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_log_message_manager.h"

#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"

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
  // During InstallPostInitializeLogHandler() there's a brief window where a
  // call into this function may be waiting on |message_lock_|, so we need to
  // check if |log_callback_| was set once we get the lock.
  if (log_callback_) {
    RouteMessage(severity, std::move(header), std::move(message));
    return;
  }

  // Otherwise just queue the message for InstallPostInitializeLogHandler() to
  // forward later.
  deferred_messages_.emplace_back(severity, std::move(header),
                                  std::move(message));
}

void GpuLogMessageManager::RouteMessage(int severity,
                                        const std::string& header,
                                        const std::string& message) {
  log_callback_.Run(severity, std::move(header), std::move(message));
}

void GpuLogMessageManager::FlushMessages(mojom::GpuHost* gpu_host) {
  base::AutoLock lock(message_lock_);
  for (auto& log : deferred_messages_) {
    gpu_host->RecordLogMessage(log.severity, std::move(log.header),
                               std::move(log.message));
  }
  deferred_messages_.clear();
}

void GpuLogMessageManager::InstallPreInitializeLogHandler() {
  DCHECK(!log_callback_);
  logging::SetLogMessageHandler(PreInitializeLogHandler);
}

void GpuLogMessageManager::InstallPostInitializeLogHandler(
    LogCallback log_callback) {
  base::AutoLock lock(message_lock_);
  DCHECK(!log_callback_);
  log_callback_ = std::move(log_callback);
  for (auto& log : deferred_messages_) {
    RouteMessage(log.severity, std::move(log.header), std::move(log.message));
  }
  deferred_messages_.clear();
  logging::SetLogMessageHandler(PostInitializeLogHandler);
}

void GpuLogMessageManager::ShutdownLogging() {
  logging::SetLogMessageHandler(nullptr);
  log_callback_.Reset();
}

}  // namespace viz
