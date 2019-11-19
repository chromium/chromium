// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace external_service_support {

class ExternalConnectorImpl : public ExternalConnector {
 public:
  explicit ExternalConnectorImpl(
      mojo::Remote<external_mojo::mojom::ExternalConnector> connector);
  explicit ExternalConnectorImpl(
      mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
          unbound_state);
  ~ExternalConnectorImpl() override;

  // ExternalConnector implementation:
  void SetConnectionErrorCallback(base::OnceClosure callback) override;
  void RegisterService(const std::string& service_name,
                       ExternalService* service) override;
  void RegisterService(
      const std::string& service_name,
      mojo::PendingRemote<external_mojo::mojom::ExternalService> service_remote)
      override;
  void BindInterface(const std::string& service_name,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  std::unique_ptr<ExternalConnector> Clone() override;
  void SendChromiumConnectorRequest(
      mojo::ScopedMessagePipeHandle request) override;
  void QueryServiceList(
      base::OnceCallback<
          void(std::vector<
               chromecast::external_mojo::mojom::ExternalServiceInfoPtr>)>
          callback) override;

 private:
  void OnMojoDisconnect();
  bool BindConnectorIfNecessary();

  mojo::Remote<external_mojo::mojom::ExternalConnector> connector_;
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector> unbound_state_;
  base::OnceClosure connection_error_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ExternalConnectorImpl);
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_EXTERNAL_CONNECTOR_IMPL_H_
