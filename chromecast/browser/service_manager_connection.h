// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_MANAGER_CONNECTION_H_
#define CHROMECAST_BROWSER_SERVICE_MANAGER_CONNECTION_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/connector.mojom-forward.h"
#include "services/service_manager/public/mojom/service.mojom-forward.h"

namespace service_manager {
class Connector;
}

namespace chromecast {

// Encapsulates a connection to a //services/service_manager.
// Access a global instance on the thread the ServiceContext was bound by
// calling Holder::Get().
// Clients can add service_manager::Service implementations whose exposed
// interfaces
// will be exposed to inbound connections to this object's Service.
// Alternatively clients can define named services that will be constructed when
// requests for those service names are received.
class ServiceManagerConnection {
 public:
  ServiceManagerConnection(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ServiceManagerConnection(const ServiceManagerConnection&) = delete;
  ServiceManagerConnection& operator=(const ServiceManagerConnection) = delete;
  ~ServiceManagerConnection();

  // Stores an instance of |connection| in TLS for the current process. Must be
  // called on the thread the connection was created on.
  static void SetForProcess(
      std::unique_ptr<ServiceManagerConnection> connection);

  // Returns the per-process instance, or nullptr if the Service Manager
  // connection has not yet been bound. Must be called on the thread the
  // connection was created on.
  static ServiceManagerConnection* GetForProcess();

  // Destroys the per-process instance. Must be called on the thread the
  // connection was created on.
  static void DestroyForProcess();

  // Creates a ServiceManagerConnection from |request|. The connection binds
  // its interfaces and accept new connections on |io_task_runner| only. Note
  // that no incoming connections are accepted until Start() is called.
  static std::unique_ptr<ServiceManagerConnection> Create(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Begins accepting incoming connections.
  void Start();

  // Returns the service_manager::Connector received via this connection's
  // Service
  // implementation. Use this to initiate connections as this object's Identity.
  service_manager::Connector* GetConnector();

 private:
  class IOThreadContext;

  void OnConnectionLost();
  void GetInterface(service_manager::mojom::InterfaceProvider* provider,
                    const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle request_handle);

  std::unique_ptr<service_manager::Connector> connector_;
  scoped_refptr<IOThreadContext> context_;
  base::WeakPtrFactory<ServiceManagerConnection> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_MANAGER_CONNECTION_H_
