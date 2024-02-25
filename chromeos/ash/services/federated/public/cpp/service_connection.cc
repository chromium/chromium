// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/federated/public/cpp/service_connection.h"

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/dbus/federated/federated_client.h"
#include "chromeos/ash/services/federated/public/mojom/federated_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash::federated {

namespace {

ServiceConnection* g_fake_service_connection_for_testing = nullptr;

// Real Impl of ServiceConnection
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();
  ServiceConnectionImpl(const ServiceConnectionImpl&) = delete;
  ServiceConnectionImpl& operator=(const ServiceConnectionImpl&) = delete;
  ~ServiceConnectionImpl() override = default;

  // ServiceConnection:
  void BindReceiver(
      mojo::PendingReceiver<chromeos::federated::mojom::FederatedService>
          receiver) override;

 private:
  // Binds the top level interface |federated_service_| to an
  // implementation in the Federated Service daemon, if it is not already bound.
  // The binding is accomplished via D-Bus bootstrap.
  void BindFederatedServiceIfNeeded();

  // Mojo disconnect handler. Resets |federated_service_|, which
  // will be reconnected upon next use.
  void OnMojoDisconnect();

  // Response callback for FederatedClient::BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(bool success);

  mojo::Remote<chromeos::federated::mojom::FederatedService> federated_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

ServiceConnectionImpl::ServiceConnectionImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ServiceConnectionImpl::BindReceiver(
    mojo::PendingReceiver<chromeos::federated::mojom::FederatedService>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindFederatedServiceIfNeeded();
  federated_service_->Clone(std::move(receiver));
}

void ServiceConnectionImpl::BindFederatedServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (federated_service_) {
    return;
  }

  mojo::PlatformChannel platform_channel;

  // Prepare a Mojo invitation to send through |platform_channel|.
  mojo::OutgoingInvitation invitation;
  // Include an initial Mojo pipe in the invitation.
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(
      ::federated::kBootstrapMojoConnectionChannelToken);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 platform_channel.TakeLocalEndpoint());

  // Bind our end of |pipe| to our mojo::Remote<FederatedService>. The daemon
  // should bind its end to a FederatedService implementation.
  federated_service_.Bind(
      mojo::PendingRemote<chromeos::federated::mojom::FederatedService>(
          std::move(pipe), 0u /* version */));
  federated_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnMojoDisconnect, base::Unretained(this)));

  // Send the file descriptor for the other end of |platform_channel| to the
  // Federated service daemon over D-Bus.
  FederatedClient::Get()->BootstrapMojoConnection(
      platform_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ServiceConnectionImpl::OnBootstrapMojoConnectionResponse,
                     base::Unretained(this)));
}

void ServiceConnectionImpl::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connection errors are not expected so log a warning.
  LOG(WARNING) << "Federated Service Mojo connection closed";
  federated_service_.reset();
}

void ServiceConnectionImpl::OnBootstrapMojoConnectionResponse(
    const bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    LOG(WARNING) << "BootstrapMojoConnection D-Bus call failed";
    federated_service_.reset();
  }
}

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  if (g_fake_service_connection_for_testing) {
    return g_fake_service_connection_for_testing;
  }
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

ScopedFakeServiceConnectionForTest::ScopedFakeServiceConnectionForTest(
    ServiceConnection* fake_service_connection) {
  DCHECK(!g_fake_service_connection_for_testing);
  g_fake_service_connection_for_testing = fake_service_connection;
}

ScopedFakeServiceConnectionForTest::~ScopedFakeServiceConnectionForTest() {
  g_fake_service_connection_for_testing = nullptr;
}

}  // namespace ash::federated
