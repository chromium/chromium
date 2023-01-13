// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/sharing/nearby/decoder/nearby_decoder.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"

namespace sharing {

SharingImpl::SharingImpl(
    mojo::PendingReceiver<mojom::Sharing> receiver,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {}

SharingImpl::~SharingImpl() {
  // No need to call DoShutDown() from the destructor because SharingImpl should
  // only be destroyed after SharingImpl::ShutDown() has been called.
  DCHECK(!nearby_connections_ && !nearby_decoder_);
}

void SharingImpl::Connect(
    NearbyDependenciesPtr deps,
    mojo::PendingReceiver<NearbyConnectionsMojom> connections_receiver,
    mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder>
        decoder_receiver) {
  DCHECK(!nearby_connections_);
  DCHECK(!nearby_decoder_);

  nearby::api::LogMessage::Severity min_log_severity = deps->min_log_severity;

  InitializeNearbySharedRemotes(std::move(deps));

  nearby_connections_ = std::make_unique<NearbyConnections>(
      std::move(connections_receiver), min_log_severity,
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kNearbyConnections));

  nearby_decoder_ =
      std::make_unique<NearbySharingDecoder>(std::move(decoder_receiver));
}

void SharingImpl::ShutDown(ShutDownCallback callback) {
  DoShutDown(/*is_expected=*/true);
  std::move(callback).Run();
}

void SharingImpl::DoShutDown(bool is_expected) {
  nearby::NearbySharedRemotes::SetInstance(nullptr);

  if (!nearby_connections_ && !nearby_decoder_)
    return;

  nearby_connections_.reset();
  nearby_decoder_.reset();

  // Leave |receiver_| valid. Its disconnection is reserved as a signal that the
  // Sharing utility process has crashed.
}

void SharingImpl::OnDisconnect(MojoDependencyName mojo_dependency_name) {
  LOG(WARNING) << "The utility process has detected that the browser process "
                  "has disconnected from a mojo pipe: ["
               << GetMojoDependencyName(mojo_dependency_name) << "]";
  base::UmaHistogramEnumeration(
      "Nearby.Connections.UtilityProcessShutdownReason."
      "DisconnectedMojoDependency",
      mojo_dependency_name);

  LOG(ERROR) << "A Sharing process dependency has unexpectedly disconnected.";
  DoShutDown(/*is_expected=*/false);
}

void SharingImpl::InitializeNearbySharedRemotes(NearbyDependenciesPtr deps) {
  nearby_shared_remotes_ = std::make_unique<nearby::NearbySharedRemotes>();
  nearby::NearbySharedRemotes::SetInstance(nearby_shared_remotes_.get());

  if (deps->bluetooth_adapter) {
    nearby_shared_remotes_->bluetooth_adapter.Bind(
        std::move(deps->bluetooth_adapter), io_task_runner_);
    nearby_shared_remotes_->bluetooth_adapter.set_disconnect_handler(
        base::BindOnce(&SharingImpl::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr(),
                       MojoDependencyName::kBluetoothAdapter),
        base::SequencedTaskRunner::GetCurrentDefault());
  }

  nearby_shared_remotes_->socket_manager.Bind(
      std::move(deps->webrtc_dependencies->socket_manager), io_task_runner_);
  nearby_shared_remotes_->socket_manager.set_disconnect_handler(
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kSocketManager),
      base::SequencedTaskRunner::GetCurrentDefault());

  nearby_shared_remotes_->mdns_responder_factory.Bind(
      std::move(deps->webrtc_dependencies->mdns_responder_factory),
      io_task_runner_);
  nearby_shared_remotes_->mdns_responder_factory.set_disconnect_handler(
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kMdnsResponder),
      base::SequencedTaskRunner::GetCurrentDefault());

  nearby_shared_remotes_->ice_config_fetcher.Bind(
      std::move(deps->webrtc_dependencies->ice_config_fetcher),
      io_task_runner_);
  nearby_shared_remotes_->ice_config_fetcher.set_disconnect_handler(
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kIceConfigFetcher),
      base::SequencedTaskRunner::GetCurrentDefault());

  nearby_shared_remotes_->webrtc_signaling_messenger.Bind(
      std::move(deps->webrtc_dependencies->messenger), io_task_runner_);
  nearby_shared_remotes_->webrtc_signaling_messenger.set_disconnect_handler(
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kWebRtcSignalingMessenger),
      base::SequencedTaskRunner::GetCurrentDefault());

  // TODO(https://crbug.com/1261238): This should always be true when the
  // WifiLan feature flag is enabled. Remove when flag is enabled by default.
  if (deps->wifilan_dependencies) {
    nearby_shared_remotes_->cros_network_config.Bind(
        std::move(deps->wifilan_dependencies->cros_network_config),
        io_task_runner_);
    nearby_shared_remotes_->cros_network_config.set_disconnect_handler(
        base::BindOnce(&SharingImpl::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr(),
                       MojoDependencyName::kCrosNetworkConfig),
        base::SequencedTaskRunner::GetCurrentDefault());

    nearby_shared_remotes_->firewall_hole_factory.Bind(
        std::move(deps->wifilan_dependencies->firewall_hole_factory),
        io_task_runner_);
    nearby_shared_remotes_->firewall_hole_factory.set_disconnect_handler(
        base::BindOnce(&SharingImpl::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr(),
                       MojoDependencyName::kFirewallHoleFactory),
        base::SequencedTaskRunner::GetCurrentDefault());

    nearby_shared_remotes_->tcp_socket_factory.Bind(
        std::move(deps->wifilan_dependencies->tcp_socket_factory),
        io_task_runner_);
    nearby_shared_remotes_->tcp_socket_factory.set_disconnect_handler(
        base::BindOnce(&SharingImpl::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr(),
                       MojoDependencyName::kTcpSocketFactory),
        base::SequencedTaskRunner::GetCurrentDefault());
  }
}

std::string SharingImpl::GetMojoDependencyName(
    MojoDependencyName dependency_name) {
  switch (dependency_name) {
    case MojoDependencyName::kNearbyConnections:
      return "Nearby Connections";
    case MojoDependencyName::kBluetoothAdapter:
      return "Bluetooth Adapter";
    case MojoDependencyName::kSocketManager:
      return "Socket Manager";
    case MojoDependencyName::kMdnsResponder:
      return "MDNS Responder";
    case MojoDependencyName::kIceConfigFetcher:
      return "ICE Config Fetcher";
    case MojoDependencyName::kWebRtcSignalingMessenger:
      return "WebRTC Signaling Messenger";
    case MojoDependencyName::kCrosNetworkConfig:
      return "CrOS Network Config";
    case MojoDependencyName::kFirewallHoleFactory:
      return "Firewall Hole Factory";
    case MojoDependencyName::kTcpSocketFactory:
      return "TCP socket Factory";
  }
}

}  // namespace sharing
