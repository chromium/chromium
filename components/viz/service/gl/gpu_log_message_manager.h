// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_GL_GPU_LOG_MESSAGE_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_GL_GPU_LOG_MESSAGE_MANAGER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/gl/gpu_logging.mojom.h"

namespace viz {
// This class is a singleton which manages LOG() message forwarding before and
// after GpuServiceImpl InitializeWithHost(). Prior to initialize, log messages
// are deferred and kept within the class. When initialized GpuServiceImpl,
// InstallPostInitializeLogHandler() will be called to flush deferred messages
// and route new ones directly to browser. This class also makes sure the
// logging mojo interface is bound to the IO thread and terminate process on the
// IO thread.
class VIZ_SERVICE_EXPORT GpuLogMessageManager {
 public:
  static GpuLogMessageManager* GetInstance();

  GpuLogMessageManager(const GpuLogMessageManager&) = delete;
  GpuLogMessageManager& operator=(const GpuLogMessageManager&) = delete;

  // Queues a deferred LOG() message into |deferred_messages_| unless
  // |should_route_messages_| has been set to true -- in which case
  // RouteMessage() is called.
  void AddDeferredMessage(int severity,
                          const std::string& header,
                          const std::string& message);

  // Used after InstallPostInitializeLogHandler() to route messages directly to
  // the GPU logging mojo interface, mojo interface send messages on IO thread.
  void RouteMessage(int severity,
                    const std::string& header,
                    const std::string& message);

  // If InstallPostInitializeLogHandler() will never be called, this method is
  // called prior to process exit to ensure logs are forwarded.
  void FlushMessages(mojom::GpuLogging* gpu_logging);

  // Used prior to GpuServiceImpl initialization during GpuMain startup to
  // ensure logs aren't lost before initialize.
  void InstallPreInitializeLogHandler();

  // Called when GpuServiceImpl is initialized, to take over logging from the
  // PostInitializeLogHandler(). Flushes all deferred messages.
  void InstallPostInitializeLogHandler(
      mojo::PendingRemote<mojom::GpuLogging> pending_remote,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Called when GpuServiceImpl is shutting down.
  void ShutdownLogging();

  // Terminate the process on IO thread and block the main thread.
  void TerminateProcess(int exit_code);

 private:
  friend class base::NoDestructor<GpuLogMessageManager>;

  GpuLogMessageManager();
  ~GpuLogMessageManager() = delete;

  // Bind GpuLogging mojo interface to specified |task_runner|.
  void Bind(mojo::PendingRemote<mojom::GpuLogging> pending_remote,
            scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  void BindOnIOThread(mojo::PendingRemote<mojom::GpuLogging> pending_remote);
  void ResetLoggingOnIOThread();

  void TerminateProcessOnIO(int exit_code);

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

  mojo::Remote<mojom::GpuLogging> gpu_logging_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_GL_GPU_LOG_MESSAGE_MANAGER_H_
