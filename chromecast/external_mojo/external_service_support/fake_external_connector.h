// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_FAKE_EXTERNAL_CONNECTOR_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_FAKE_EXTERNAL_CONNECTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "chromecast/external_mojo/public/mojom/test_connector.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {
namespace external_service_support {

// Local, single-process ExternalConnector for testing. Clients can register
// mock services with FakeExternalConnector and verify that tested code makes
// the expected service requests.
class FakeExternalConnector
    : public external_service_support::ExternalConnector,
      public external_mojo::mojom::TestExternalConnector {
 public:
  FakeExternalConnector();
  explicit FakeExternalConnector(
      mojo::PendingRemote<external_mojo::mojom::TestExternalConnector> remote);
  FakeExternalConnector(const FakeExternalConnector&) = delete;
  FakeExternalConnector& operator=(const FakeExternalConnector&) = delete;
  ~FakeExternalConnector() override;

  // external_service_support::ExternalConnector implementation:
  void RegisterService(const std::string& service_name,
                       ExternalService* service) override;
  void RegisterService(
      const std::string& service_name,
      mojo::PendingRemote<external_mojo::mojom::ExternalService> service_remote)
      override;
  void RegisterServices(const std::vector<std::string>& service_names,
                        const std::vector<ExternalService*>& services) override;
  void BindInterface(const std::string& service_name,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     bool async = true) override;
  // Note: These methods are unimplemented.
  base::CallbackListSubscription AddConnectionErrorCallback(
      base::RepeatingClosure callback) override;
  void RegisterServices(
      std::vector<chromecast::external_mojo::mojom::ServiceInstanceInfoPtr>
          service_instances_info) override;
  std::unique_ptr<external_service_support::ExternalConnector> Clone() override;
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
  // external_mojo::mojom::TestExternalConnector implementation:
  void BindInterfaceInternal(
      const std::string& service_name,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) override;

  mojo::Remote<external_mojo::mojom::TestExternalConnector> parent_;
  base::flat_map<std::string,
                 mojo::Remote<external_mojo::mojom::ExternalService>>
      services_;
  mojo::ReceiverSet<external_mojo::mojom::TestExternalConnector>
      child_receivers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_FAKE_EXTERNAL_CONNECTOR_H_
