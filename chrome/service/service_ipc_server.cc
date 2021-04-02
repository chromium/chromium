// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_ipc_server.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_delta_serialization.h"

ServiceIPCServer::ServiceIPCServer(
    Client* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    base::WaitableEvent* shutdown_event)
    : client_(client),
      io_task_runner_(io_task_runner),
      shutdown_event_(shutdown_event) {
  DCHECK(client);
  DCHECK(shutdown_event);
  binder_registry_.AddInterface(
      base::BindRepeating(&ServiceIPCServer::HandleServiceProcessConnection,
                          base::Unretained(this)));
}

bool ServiceIPCServer::Init() {
  CreateChannel();
  return true;
}

void ServiceIPCServer::CreateChannel() {
  receiver_.reset();

  receiver_.Bind(
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>(
          client_->CreateChannelMessagePipe()));
  receiver_.set_disconnect_handler(base::BindOnce(
      &ServiceIPCServer::OnChannelError, base::Unretained(this)));
}

ServiceIPCServer::~ServiceIPCServer() = default;

void ServiceIPCServer::OnChannelError() {
  // When an IPC client (typically a browser process) disconnects, the pipe is
  // closed and we get an OnChannelError. If we want to keep servicing requests,
  // we will recreate the channel.
  bool client_was_connected = ipc_client_connected_;
  ipc_client_connected_ = false;
  if (client_was_connected) {
    if (client_->OnIPCClientDisconnect())
      CreateChannel();
  } else if (!ipc_client_connected_) {
    // If the client was never even connected we had an error connecting.
    LOG(ERROR) << "Unable to open service ipc channel";
  }
}

void ServiceIPCServer::Hello(HelloCallback callback) {
  ipc_client_connected_ = true;
  std::move(callback).Run();
}

void ServiceIPCServer::GetHistograms(GetHistogramsCallback callback) {
  if (!histogram_delta_serializer_) {
    histogram_delta_serializer_ =
        std::make_unique<base::HistogramDeltaSerialization>("ServiceProcess");
  }
  std::vector<std::string> deltas;
  // "false" to PerpareAndSerializeDeltas() indicates to *not* include
  // histograms held in persistent storage on the assumption that they will be
  // visible to the recipient through other means.
  histogram_delta_serializer_->PrepareAndSerializeDeltas(&deltas, false);
  std::move(callback).Run(deltas);
}

void ServiceIPCServer::ShutDown() {
  client_->OnShutdown();
}

void ServiceIPCServer::UpdateAvailable() {
  client_->OnUpdateAvailable();
}

void ServiceIPCServer::GetInterface(const std::string& interface_name,
                                    mojo::ScopedMessagePipeHandle pipe) {
  binder_registry_.BindInterface(interface_name, std::move(pipe));
}

void ServiceIPCServer::HandleServiceProcessConnection(
    mojo::PendingReceiver<chrome::mojom::ServiceProcess> receiver) {
  service_process_receivers_.Add(this, std::move(receiver));
}
