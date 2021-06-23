// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_IPC_SERVER_H_
#define CHROME_SERVICE_SERVICE_IPC_SERVER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chrome/common/service_process.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace base {

class HistogramDeltaSerialization;
class WaitableEvent;

}  // namespace base

// This class handles IPC commands for the service process.
class ServiceIPCServer : public service_manager::mojom::InterfaceProvider,
                         public chrome::mojom::ServiceProcess {
 public:
  class Client {
   public:
    virtual ~Client() {}

    // Called when the service process must shut down.
    virtual void OnShutdown() = 0;

    // Called when a product update is available.
    virtual void OnUpdateAvailable() = 0;

    // Called when the IPC channel is closed. A return value of true indicates
    // that the IPC server should continue listening for new connections.
    virtual bool OnIPCClientDisconnect() = 0;

    // Called to create a message pipe to use for an IPC Channel connection.
    virtual mojo::ScopedMessagePipeHandle CreateChannelMessagePipe() = 0;
  };

  ServiceIPCServer(
      Client* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      base::WaitableEvent* shutdown_event);
  ~ServiceIPCServer() override;

  bool Init();

  service_manager::BinderRegistry& binder_registry() {
    return binder_registry_;
  }

  bool is_ipc_client_connected() const { return ipc_client_connected_; }

 private:
  friend class ServiceIPCServerTest;
  friend class MockServiceIPCServer;

  // Helper method to create the sync channel.
  void CreateChannel();

  void OnChannelError();

  // chrome::mojom::ServiceProcess:
  void Hello(HelloCallback callback) override;
  void GetHistograms(GetHistogramsCallback callback) override;
  void UpdateAvailable() override;
  void ShutDown() override;

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle pipe) override;

  void HandleServiceProcessConnection(
      mojo::PendingReceiver<chrome::mojom::ServiceProcess> receiver);

  Client* client_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  base::WaitableEvent* shutdown_event_;

  // Indicates whether an IPC client is currently connected to the channel.
  bool ipc_client_connected_ = false;

  // Calculates histograms deltas.
  std::unique_ptr<base::HistogramDeltaSerialization>
      histogram_delta_serializer_;

  mojo::Receiver<service_manager::mojom::InterfaceProvider> receiver_{this};
  mojo::ReceiverSet<chrome::mojom::ServiceProcess> service_process_receivers_;

  service_manager::BinderRegistry binder_registry_;

  DISALLOW_COPY_AND_ASSIGN(ServiceIPCServer);
};

#endif  // CHROME_SERVICE_SERVICE_IPC_SERVER_H_
