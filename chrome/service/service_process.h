// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_PROCESS_H_
#define CHROME_SERVICE_SERVICE_PROCESS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/net/in_process_network_connection_tracker.h"
#include "chrome/service/service_ipc_server.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/message_pipe.h"

class ServiceProcessPrefs;
class ServiceURLRequestContextGetter;
class ServiceProcessState;

namespace base {
class CommandLine;
class WaitableEvent;
}

namespace mojo {
class IsolatedConnection;
namespace core {
class ScopedIPCSupport;
}
}

namespace net {
class NetworkChangeNotifier;
}

// The ServiceProcess does not inherit from ChildProcess because this
// process can live independently of the browser process.
// ServiceProcess Design Notes
// https://sites.google.com/a/chromium.org/dev/developers/design-documents/service-processes
class ServiceProcess : public ServiceIPCServer::Client,
                       public cloud_print::CloudPrintProxy::Client,
                       public cloud_print::CloudPrintProxy::Provider {
 public:
  ServiceProcess();
  ~ServiceProcess() override;

  // Initialize the ServiceProcess. |quit_closure| will be run when the service
  // process is ready to exit.
  bool Initialize(base::OnceClosure quit_closure,
                  const base::CommandLine& command_line,
                  std::unique_ptr<ServiceProcessState> state);

  bool Teardown();

  // Returns the SingleThreadTaskRunner for the service process io thread (used
  // for e.g. network requests and IPC). Returns null before Initialize is
  // called and after Teardown is called.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() {
    return io_thread_ ? io_thread_->task_runner() : nullptr;
  }

  // A global event object that is signalled when the main thread's message
  // loop exits. This gives background threads a way to observe the main
  // thread shutting down.
  base::WaitableEvent* GetShutdownEventForTesting() {
    return &shutdown_event_;
  }

  // Shuts down the service process.
  void Shutdown();

  // ServiceIPCServer::Client implementation.
  void OnShutdown() override;
  void OnUpdateAvailable() override;
  bool OnIPCClientDisconnect() override;
  mojo::ScopedMessagePipeHandle CreateChannelMessagePipe() override;

  // CloudPrintProxy::Provider implementation.
  cloud_print::CloudPrintProxy* GetCloudPrintProxy() override;

  // CloudPrintProxy::Client implementation.
  void OnCloudPrintProxyEnabled(bool persist_state) override;
  void OnCloudPrintProxyDisabled(bool persist_state) override;

  ServiceURLRequestContextGetter* GetServiceURLRequestContextGetter();

 private:
  friend class TestServiceProcess;

  // Schedule a call to ShutdownIfNeeded.
  void ScheduleShutdownCheck();

  // Shuts down the process if no services are enabled and no IPC client is
  // connected.
  void ShutdownIfNeeded();

  // Called exactly ONCE per process instance for each service that gets
  // enabled in this process.
  void OnServiceEnabled();

  // Called exactly ONCE per process instance for each service that gets
  // disabled in this process (note that shutdown != disabled).
  void OnServiceDisabled();

  // Terminate forces the service process to quit.
  void Terminate();

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  std::unique_ptr<InProcessNetworkConnectionTracker>
      network_connection_tracker_;
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<cloud_print::CloudPrintProxy> cloud_print_proxy_;
  std::unique_ptr<ServiceProcessPrefs> service_prefs_;
  std::unique_ptr<ServiceIPCServer> ipc_server_;
  std::unique_ptr<ServiceProcessState> service_process_state_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> mojo_ipc_support_;
  std::unique_ptr<mojo::IsolatedConnection> mojo_connection_;

  // An event that will be signalled when we shutdown.
  base::WaitableEvent shutdown_event_;

  // Closure to run to cause the main message loop to exit.
  base::OnceClosure quit_closure_;

  // Count of currently enabled services in this process.
  int enabled_services_;

  // Speficies whether a product update is available.
  bool update_available_;

  scoped_refptr<ServiceURLRequestContextGetter> request_context_getter_;

#if defined(OS_POSIX)
  mojo::PlatformChannelServerEndpoint server_endpoint_;
#elif defined(OS_WIN)
  mojo::NamedPlatformChannel::ServerName server_name_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ServiceProcess);
};

extern ServiceProcess* g_service_process;

#endif  // CHROME_SERVICE_SERVICE_PROCESS_H_
