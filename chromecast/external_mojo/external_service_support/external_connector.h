// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "chromecast/external_mojo/public/mojom/connector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromecast {
namespace external_service_support {
class ExternalService;

// Interface to the Mojo broker, allowing services to be registered and other
// processes to bind to registered services. Once any public methods are called
// on an instance of this class, that instance is bound to the calling thread.
//
// To use the same interface on another thread, call Clone() and pass the new
// instance to the desired thread before calling any public methods on it.
class ExternalConnector {
 public:
  static void Connect(
      const std::string& broker_path,
      base::OnceCallback<void(std::unique_ptr<ExternalConnector>)> callback);

  static std::unique_ptr<ExternalConnector> Create(
      const std::string& broker_path);

  static std::unique_ptr<ExternalConnector> Create(
      mojo::PendingRemote<external_mojo::mojom::ExternalConnector> remote);

  // Acquires a connector from the BrokerService via the Chromium service
  // manager.
  static std::unique_ptr<ExternalConnector> Create(
      ::service_manager::Connector* connector);

  virtual ~ExternalConnector() = default;

  // Adds a callback that will be called if this class loses its connection to
  // the Mojo broker. The calling class must retain the returned subscription
  // until it intends to unregister. By the time |callback| is executed, a new
  // attempt at connecting will be started, and this object is valid. Note that
  // some prior messages may be lost.
  [[nodiscard]] virtual base::CallbackListSubscription
  AddConnectionErrorCallback(base::RepeatingClosure callback) = 0;

  // Registers a service that other Mojo processes/services can bind to. Others
  // can call BindInterface(|service_name|, interface_name) to bind to this
  // |service|.
  // If registering multiple services, consider using RegisterServices().
  virtual void RegisterService(const std::string& service_name,
                               ExternalService* service) = 0;
  virtual void RegisterService(
      const std::string& service_name,
      mojo::PendingRemote<external_mojo::mojom::ExternalService>
          service_remote) = 0;

  // Registers multiple services that other Mojo processes/services can bind to.
  // Others can call BindInterface(|service_names[i]|, interface_name) to bind
  // to this |service[i]|.
  // This function is more efficient than using multiple times RegisterService()
  // because it only does a single Mojo call.
  virtual void RegisterServices(
      const std::vector<std::string>& service_names,
      const std::vector<ExternalService*>& services) = 0;
  virtual void RegisterServices(
      std::vector<chromecast::external_mojo::mojom::ServiceInstanceInfoPtr>
          service_instances_info) = 0;

  // Asks the Mojo broker to bind to a matching interface on the service with
  // the given |service_name|. If the service does not yet exist, the binding
  // will remain in progress until the service is registered. If |async| is
  // |false|, then the bind will execute synchronously; otherwise, it will
  // execute asynchronously on the same sequence (see b/146508043).
  template <typename Interface>
  void BindInterface(const std::string& service_name,
                     mojo::PendingReceiver<Interface> receiver,
                     bool async = true) {
    BindInterface(service_name, Interface::Name_, receiver.PassPipe(), async);
  }

  virtual void BindInterface(const std::string& service_name,
                             const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle interface_pipe,
                             bool async = true) = 0;

  // Creates a new instance of this class which may be passed to another thread.
  // The returned object may be passed across sequences until any of its public
  // methods are called, at which point it becomes bound to the calling
  // sequence.
  virtual std::unique_ptr<ExternalConnector> Clone() = 0;

  // Requests a PendingRemote for an ExternalConnector which can be passed to a
  // different process.
  virtual mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
  RequestConnector() = 0;

  // Sends a request for a Chromium ServiceManager connector.
  virtual void SendChromiumConnectorRequest(
      mojo::ScopedMessagePipeHandle request) = 0;

  // Query the list of available services from this connector.
  virtual void QueryServiceList(
      base::OnceCallback<
          void(std::vector<
               chromecast::external_mojo::mojom::ExternalServiceInfoPtr>)>
          callback) = 0;
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_H_
