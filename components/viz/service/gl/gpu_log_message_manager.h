// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_GL_GPU_LOG_MESSAGE_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_GL_GPU_LOG_MESSAGE_MANAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

namespace mojom {
class GpuHost;
}

// Class which manages LOG() message forwarding before and after GpuServiceImpl
// InitializeWithHost(). Prior to initialize, log messages are deferred and kept
// within the class. During initialize, InstallPostInitializeLogHandler() will
// be called to flush deferred messages and route new ones directly to GpuHost.
class VIZ_SERVICE_EXPORT GpuLogMessageManager {
 public:
  using LogCallback = base::RepeatingCallback<void(int severity,
                                                   const std::string& header,
                                                   const std::string& message)>;

  static GpuLogMessageManager* GetInstance();

  GpuLogMessageManager(const GpuLogMessageManager&) = delete;
  GpuLogMessageManager& operator=(const GpuLogMessageManager&) = delete;

  // Queues a deferred LOG() message into |deferred_messages_| unless
  // |log_callback_| has been set -- in which case RouteMessage() is called.
  void AddDeferredMessage(int severity,
                          const std::string& header,
                          const std::string& message);

  // Used after InstallPostInitializeLogHandler() to route messages directly to
  // |log_callback_|; avoids the need for a global lock.
  void RouteMessage(int severity,
                    const std::string& header,
                    const std::string& message);

  // If InstallPostInitializeLogHandler() will never be called, this method is
  // called prior to process exit to ensure logs are forwarded.
  void FlushMessages(mojom::GpuHost* gpu_host);

  // Used prior to InitializeWithHost() during GpuMain startup to ensure logs
  // aren't lost before initialize.
  void InstallPreInitializeLogHandler();

  // Called when GPUService created to take over logging from the
  // PostInitializeLogHandler(). Flushes all deferred messages.
  void InstallPostInitializeLogHandler(LogCallback log_callback);

  // Called when it's no longer safe to invoke |log_callback_|.
  void ShutdownLogging();

 private:
  friend class base::NoDestructor<GpuLogMessageManager>;

  GpuLogMessageManager();
  ~GpuLogMessageManager() = delete;

  struct LogMessage {
    LogMessage(int severity,
               const std::string& header,
               const std::string& message)
        : severity(severity),
          header(std::move(header)),
          message(std::move(message)) {}
    const int severity;
    const std::string header;
    const std::string message;
  };

  base::Lock message_lock_;
  std::vector<LogMessage> deferred_messages_ GUARDED_BY(message_lock_);

  // Set once under |mesage_lock_|, but may be accessed without lock after that.
  LogCallback log_callback_;
};

}  // namespace viz
#endif  // COMPONENTS_VIZ_SERVICE_GL_GPU_LOG_MESSAGE_MANAGER_H_
