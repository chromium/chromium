// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/chromebox_for_meetings/public/cpp/service_connection.h"

#include <memory>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace ash {
namespace cfm {

namespace {

// TODO(https://crbug.com/1164001): remove after migrating to ash.
namespace mojom = ::chromeos::cfm::mojom;

constexpr char kPlatformErrorMessage[] = "CfmServiceContext bootstrap failed: ";

// Real Impl of ServiceConnection.
// Wraps |CfmServiceContext| to allow a single mojo invitation to facilitate
// multiple |CfmServiceContext|s.
class ServiceConnectionImpl : public ServiceConnection,
                              mojom::CfmServiceContext {
 public:
  ServiceConnectionImpl();
  ServiceConnectionImpl(const ServiceConnectionImpl&) = delete;
  ServiceConnectionImpl& operator=(const ServiceConnectionImpl&) = delete;
  ~ServiceConnectionImpl() override = default;

 private:
  void BindServiceContext(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver) override;

  // Binds the |CfmServiceContext| if needed.
  void BindPlatformServiceContextIfNeeded();

  void CfMContextServiceStarted(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver,
      bool is_available);

  // Response callback for CfmHotlineClient::BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver,
      mojo::ScopedMessagePipeHandle context_remote_pipe,
      bool success);

  // |mojom::CfmServiceContext| implementation.
  void ProvideAdaptor(
      const std::string& interface_name,
      mojo::PendingRemote<mojom::CfmServiceAdaptor> adaptor_remote,
      ProvideAdaptorCallback callback) override;
  void RequestBindService(const std::string& interface_name,
                          mojo::ScopedMessagePipeHandle receiver_pipe,
                          RequestBindServiceCallback callback) override;

  void OnMojoConnectionError();

  mojo::Remote<mojom::CfmServiceContext> remote_;
  mojo::ReceiverSet<mojom::CfmServiceContext> receiver_set_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ServiceConnectionImpl> weak_factory_{this};
};

void ServiceConnectionImpl::BindServiceContext(
    mojo::PendingReceiver<mojom::CfmServiceContext> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BindPlatformServiceContextIfNeeded();

  receiver_set_.Add(this, std::move(receiver));
  VLOG(2) << "Bound |CfmServiceContext| Request";
}

void ServiceConnectionImpl::BindPlatformServiceContextIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remote_.is_bound()) {
    return;
  }

  auto pending_receiver = remote_.BindNewPipeAndPassReceiver();
  remote_.reset_on_disconnect();
  // Note: Bind the remote so called can be queued
  CfmHotlineClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&ServiceConnectionImpl::CfMContextServiceStarted,
                     weak_factory_.GetWeakPtr(), std::move(pending_receiver)));
}

void ServiceConnectionImpl::CfMContextServiceStarted(
    mojo::PendingReceiver<mojom::CfmServiceContext> receiver,
    bool is_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_available) {
    LOG(WARNING) << kPlatformErrorMessage << "Service not available.";
    receiver.reset();
    return;
  }

  mojo::PlatformChannel channel;

  // Invite the Chromium OS service to the Chromium IPC network
  // Prepare a Mojo invitation to send through |platform_channel|.
  mojo::OutgoingInvitation invitation;
  // Include an initial Mojo pipe in the invitation.
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(0u);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());

  // Send the file descriptor for the other end of |platform_channel| to the
  // Cfm service daemon over D-Bus.
  CfmHotlineClient::Get()->BootstrapMojoConnection(
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ServiceConnectionImpl::OnBootstrapMojoConnectionResponse,
                     weak_factory_.GetWeakPtr(), std::move(receiver),
                     std::move(pipe)));
}

void ServiceConnectionImpl::OnBootstrapMojoConnectionResponse(
    mojo::PendingReceiver<mojom::CfmServiceContext> receiver,
    mojo::ScopedMessagePipeHandle context_remote_pipe,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    LOG(WARNING) << kPlatformErrorMessage
                 << "BootstrapMojoConnection D-Bus call failed";
    receiver.reset();
    return;
  }

  MojoResult result = mojo::FuseMessagePipes(receiver.PassPipe(),
                                             std::move(context_remote_pipe));
  if (result != MOJO_RESULT_OK) {
    LOG(WARNING) << kPlatformErrorMessage << "Fusing message pipes failed.";
  } else {
    VLOG(2) << "Hotline Service Ready.";
  }
}

void ServiceConnectionImpl::ProvideAdaptor(
    const std::string& interface_name,
    mojo::PendingRemote<mojom::CfmServiceAdaptor> adaptor_remote,
    ProvideAdaptorCallback callback) {
  BindPlatformServiceContextIfNeeded();

  remote_->ProvideAdaptor(std::move(interface_name), std::move(adaptor_remote),
                          std::move(callback));
}

void ServiceConnectionImpl::RequestBindService(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiver_pipe,
    RequestBindServiceCallback callback) {
  BindPlatformServiceContextIfNeeded();

  remote_->RequestBindService(std::move(interface_name),
                              std::move(receiver_pipe), std::move(callback));
}

void ServiceConnectionImpl::OnMojoConnectionError() {
  // The lifecycle of connected clients is unimportant since this class
  // ultimately behaves like a one-off factory class
  VLOG(2) << "Connection to factory close received";
}

ServiceConnectionImpl::ServiceConnectionImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  receiver_set_.set_disconnect_handler(
      base::BindRepeating(&ServiceConnectionImpl::OnMojoConnectionError,
                          weak_factory_.GetWeakPtr()));
}

static ServiceConnection* g_fake_service_connection_for_testing = nullptr;

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  if (g_fake_service_connection_for_testing) {
    return g_fake_service_connection_for_testing;
  }
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

void ServiceConnection::UseFakeServiceConnectionForTesting(
    ServiceConnection* const fake_service_connection) {
  g_fake_service_connection_for_testing = fake_service_connection;
}

}  // namespace cfm
}  // namespace ash
