// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/chromebox_for_meetings/features.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace ash {
namespace cfm {

namespace {

namespace mojom = ::chromeos::cfm::mojom;

constexpr char kPlatformErrorMessage[] = "CfmServiceContext bootstrap failed: ";

// Real Impl of ServiceConnection for ash built for Chromebox For Meetings.
// Wraps |CfmServiceContext| to allow a single mojo invitation to facilitate
// multiple |CfmServiceContext|s.
// Note: This impl is usded when ash is compiled with the |is_cfm| build flag.
class COMPONENT_EXPORT(CHROMEOS_CFMSERVICE) ServiceConnectionCfmAshImpl
    : public chromeos::cfm::ServiceConnection, mojom::CfmServiceContext {
 public:
  ServiceConnectionCfmAshImpl();
  ServiceConnectionCfmAshImpl(const ServiceConnectionCfmAshImpl&) = delete;
  ServiceConnectionCfmAshImpl& operator=(
      const ServiceConnectionCfmAshImpl&) = delete;
  ~ServiceConnectionCfmAshImpl() override = default;

  // Binds a |CfMServiceContext| receiver to this implementation in order to
  // forward requests to the underlying daemon connected by a single remote.
  void BindServiceContext(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver) override;

 private:
  // Binds the primary |CfmServiceContext| remote to the underlying daemon so
  // that the connected reciever set can forward requests only if the remote is
  // not already bound.
  void BindPlatformServiceContextIfNeeded();

  void CfMContextServiceStarted(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver,
      bool is_available);

  // Response callback for CfmHotlineClient::BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver,
      mojo::ScopedMessagePipeHandle context_remote_pipe,
      bool success);

  // mojom::CfmServiceContext:
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
  base::WeakPtrFactory<ServiceConnectionCfmAshImpl> weak_factory_{this};
};

void ServiceConnectionCfmAshImpl::BindServiceContext(
    mojo::PendingReceiver<mojom::CfmServiceContext> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If experiment is disabled reset receiver
  if (!base::FeatureList::IsEnabled(ash::cfm::features::kMojoServices)) {
    receiver.ResetWithReason(
        static_cast<uint32_t>(
            chromeos::cfm::mojom::DisconnectReason::kFinchDisabledCode),
        chromeos::cfm::mojom::DisconnectReason::kFinchDisabledMessage);

    VLOG(2) << "CfMServiceContext disabled, receiver reset";
    return;
  }

  BindPlatformServiceContextIfNeeded();

  receiver_set_.Add(this, std::move(receiver));
  VLOG(2) << "Bound |CfmServiceContext| Request";
}

void ServiceConnectionCfmAshImpl::BindPlatformServiceContextIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remote_.is_bound()) {
    return;
  }

  auto pending_receiver = remote_.BindNewPipeAndPassReceiver();
  remote_.reset_on_disconnect();
  // Note: Bind the remote so calls can be queued
  CfmHotlineClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&ServiceConnectionCfmAshImpl::CfMContextServiceStarted,
                     weak_factory_.GetWeakPtr(), std::move(pending_receiver)));
}

void ServiceConnectionCfmAshImpl::CfMContextServiceStarted(
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
      base::BindOnce(
          &ServiceConnectionCfmAshImpl::OnBootstrapMojoConnectionResponse,
          weak_factory_.GetWeakPtr(), std::move(receiver),
          std::move(pipe)));
}

void ServiceConnectionCfmAshImpl::OnBootstrapMojoConnectionResponse(
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

void ServiceConnectionCfmAshImpl::ProvideAdaptor(
    const std::string& interface_name,
    mojo::PendingRemote<mojom::CfmServiceAdaptor> adaptor_remote,
    ProvideAdaptorCallback callback) {
  BindPlatformServiceContextIfNeeded();

  // Wrap callback with default invoke to correctly report a failure in the
  // event of an unsuccessful bootstrap.
  remote_->ProvideAdaptor(
      std::move(interface_name), std::move(adaptor_remote),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void ServiceConnectionCfmAshImpl::RequestBindService(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiver_pipe,
    RequestBindServiceCallback callback) {
  BindPlatformServiceContextIfNeeded();

  // Wrap callback with default invoke to correctly report a failure in the
  // event of an unsuccessful bootstrap.
  remote_->RequestBindService(
      std::move(interface_name), std::move(receiver_pipe),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
}

void ServiceConnectionCfmAshImpl::OnMojoConnectionError() {
  // The lifecycle of connected clients is unimportant since this class
  // ultimately behaves like a one-off factory class
  VLOG(2) << "Connection to factory close received";
}

ServiceConnectionCfmAshImpl::ServiceConnectionCfmAshImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  receiver_set_.set_disconnect_handler(
      base::BindRepeating(&ServiceConnectionCfmAshImpl::OnMojoConnectionError,
                          weak_factory_.GetWeakPtr()));
}

}  // namespace

}  // namespace cfm
}  // namespace ash

namespace chromeos {
namespace cfm {

ServiceConnection* ServiceConnection::GetInstanceForCurrentPlatform() {
  static base::NoDestructor<ash::cfm::ServiceConnectionCfmAshImpl>
      service_connection;
  return service_connection.get();
}

}  // namespace cfm
}  // namespace chromeos
