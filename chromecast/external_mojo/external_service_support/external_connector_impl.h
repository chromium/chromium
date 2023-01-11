// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace external_service_support {

class ExternalConnectorImpl : public ExternalConnector {
  class BrokerConnection;

 public:
  explicit ExternalConnectorImpl(const std::string& broker_path);
  explicit ExternalConnectorImpl(
      scoped_refptr<BrokerConnection> broker_connection);
  // For in-process connectors only.
  explicit ExternalConnectorImpl(
      mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
          pending_remote);

  ExternalConnectorImpl(const ExternalConnectorImpl&) = delete;
  ExternalConnectorImpl& operator=(const ExternalConnectorImpl&) = delete;

  ~ExternalConnectorImpl() override;

  // ExternalConnector implementation:
  base::CallbackListSubscription AddConnectionErrorCallback(
      base::RepeatingClosure callback) override;
  void RegisterService(const std::string& service_name,
                       ExternalService* service) override;
  void RegisterService(
      const std::string& service_name,
      mojo::PendingRemote<external_mojo::mojom::ExternalService> service_remote)
      override;
  void RegisterServices(const std::vector<std::string>& service_names,
                        const std::vector<ExternalService*>& services) override;
  void RegisterServices(
      std::vector<chromecast::external_mojo::mojom::ServiceInstanceInfoPtr>
          service_instances_info) override;
  void BindInterface(const std::string& service_name,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     bool async = true) override;
  std::unique_ptr<ExternalConnector> Clone() override;
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
  RequestConnector() override;
  void SendChromiumConnectorRequest(
      mojo::ScopedMessagePipeHandle request) override;
  void QueryServiceList(
      base::OnceCallback<
          void(std::vector<
               chromecast::external_mojo::mojom::ExternalServiceInfoPtr>)>
          callback) override;

 private:
  void BindInterfaceImmediately(const std::string& service_name,
                                const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe);
  void Connect();
  void OnMojoDisconnect();
  void BindConnectorIfNecessary();

  const scoped_refptr<BrokerConnection> broker_connection_;

  int64_t connection_token_ = 0;
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector> pending_remote_;
  mojo::Remote<external_mojo::mojom::ExternalConnector> connector_;

  base::RepeatingClosureList error_closures_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ExternalConnectorImpl> weak_factory_{this};
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_
