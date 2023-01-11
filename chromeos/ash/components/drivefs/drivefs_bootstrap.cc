// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_bootstrap.h"

#include <utility>

#include "base/functional/bind.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"

namespace drivefs {

DriveFsBootstrapListener::DriveFsBootstrapListener()
    : bootstrap_(invitation_.AttachMessagePipe("drivefs-bootstrap"),
                 mojom::DriveFsBootstrap::Version_),
      pending_token_(base::UnguessableToken::Create()) {
  mojo_bootstrap::PendingConnectionManager::Get().ExpectOpenIpcChannel(
      pending_token_,
      base::BindOnce(&DriveFsBootstrapListener::AcceptMojoConnection,
                     base::Unretained(this)));
}

DriveFsBootstrapListener::~DriveFsBootstrapListener() {
  if (pending_token_) {
    mojo_bootstrap::PendingConnectionManager::Get()
        .CancelExpectedOpenIpcChannel(pending_token_);
    pending_token_ = {};
  }
}

mojo::PendingRemote<mojom::DriveFsBootstrap>
DriveFsBootstrapListener::bootstrap() {
  return std::move(bootstrap_);
}

void DriveFsBootstrapListener::AcceptMojoConnection(base::ScopedFD handle) {
  DCHECK(pending_token_);
  pending_token_ = {};
  connected_ = true;
  SendInvitationOverPipe(std::move(handle));
}

void DriveFsBootstrapListener::SendInvitationOverPipe(base::ScopedFD handle) {
  mojo::OutgoingInvitation::Send(
      std::move(invitation_), base::kNullProcessHandle,
      mojo::PlatformChannelEndpoint(mojo::PlatformHandle(std::move(handle))));
}

class DriveFsConnectionImpl : public DriveFsConnection {
 public:
  DriveFsConnectionImpl(
      std::unique_ptr<DriveFsBootstrapListener> bootstrap_listener,
      mojom::DriveFsConfigurationPtr config)
      : bootstrap_listener_(std::move(bootstrap_listener)),
        config_(std::move(config)) {}

  DriveFsConnectionImpl(const DriveFsConnectionImpl&) = delete;
  DriveFsConnectionImpl& operator=(const DriveFsConnectionImpl&) = delete;

  ~DriveFsConnectionImpl() override = default;

  base::UnguessableToken Connect(mojom::DriveFsDelegate* delegate,
                                 base::OnceClosure on_disconnected) override {
    delegate_receiver_ =
        std::make_unique<mojo::Receiver<mojom::DriveFsDelegate>>(delegate);
    on_disconnected_ = std::move(on_disconnected);

    auto bootstrap =
        mojo::Remote<mojom::DriveFsBootstrap>(bootstrap_listener_->bootstrap());
    auto token = bootstrap_listener_->pending_token();

    bootstrap->Init(std::move(config_), drivefs_.BindNewPipeAndPassReceiver(),
                    delegate_receiver_->BindNewPipeAndPassRemote());

    delegate_receiver_->set_disconnect_handler(base::BindOnce(
        &DriveFsConnectionImpl::OnMojoConnectionError, base::Unretained(this)));
    drivefs_.set_disconnect_handler(base::BindOnce(
        &DriveFsConnectionImpl::OnMojoConnectionError, base::Unretained(this)));

    return token;
  }

  mojom::DriveFs& GetDriveFs() override {
    CHECK(drivefs_);
    return *drivefs_;
  }

 private:
  void OnMojoConnectionError() {
    if (on_disconnected_ && bootstrap_listener_->is_connected()) {
      std::move(on_disconnected_).Run();
    }
  }

  const std::unique_ptr<DriveFsBootstrapListener> bootstrap_listener_;
  mojom::DriveFsConfigurationPtr config_;

  std::unique_ptr<mojo::Receiver<mojom::DriveFsDelegate>> delegate_receiver_;
  mojo::Remote<mojom::DriveFs> drivefs_;

  base::OnceClosure on_disconnected_;
};

std::unique_ptr<DriveFsConnection> DriveFsConnection::Create(
    std::unique_ptr<DriveFsBootstrapListener> bootstrap_listener,
    mojom::DriveFsConfigurationPtr config) {
  return std::make_unique<DriveFsConnectionImpl>(std::move(bootstrap_listener),
                                                 std::move(config));
}
}  // namespace drivefs
