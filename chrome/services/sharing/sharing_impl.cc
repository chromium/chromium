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
#include "chrome/services/sharing/nearby/nearby_presence.h"
#include "chrome/services/sharing/nearby/quick_start_decoder/quick_start_decoder.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"

namespace sharing {

SharingImpl::SharingImpl(
    mojo::PendingReceiver<mojom::Sharing> receiver,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {}

SharingImpl::~SharingImpl() {
  // No need to call DoShutDown() from the destructor because SharingImpl should
  // only be destroyed after SharingImpl::ShutDown() has been called.
  CHECK(!nearby_connections_ && !nearby_presence_ && !nearby_decoder_);
}

void SharingImpl::Connect(
    NearbyDependenciesPtr deps,
    mojo::PendingReceiver<NearbyConnectionsMojom> connections_receiver,
    mojo::PendingReceiver<NearbyPresenceMojom> presence_receiver,
    mojo::PendingReceiver<::sharing::mojom::NearbySharingDecoder>
        decoder_receiver,
    mojo::PendingReceiver<ash::quick_start::mojom::QuickStartDecoder>
        quick_start_decoder_receiver) {
  CHECK(!nearby_connections_);
  CHECK(!nearby_presence_);
  CHECK(!nearby_decoder_);

  nearby::api::LogMessage::Severity min_log_severity = deps->min_log_severity;

  InitializeNearbySharedRemotes(std::move(deps));

  nearby_presence_ = std::make_unique<NearbyPresence>(
      std::move(presence_receiver),
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kNearbyPresence));

  nearby_connections_ = std::make_unique<NearbyConnections>(
      std::move(connections_receiver),
      nearby_presence_->GetLocalDeviceProvider(), min_log_severity,
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kNearbyConnections));

  nearby_decoder_ = std::make_unique<NearbySharingDecoder>(
      std::move(decoder_receiver),
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kNearbyShareDecoder));

  quick_start_decoder_ = std::make_unique<ash::quick_start::QuickStartDecoder>(
      std::move(quick_start_decoder_receiver),
      base::BindOnce(&SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                     MojoDependencyName::kQuickStartDecoder));
}

void SharingImpl::ShutDown(ShutDownCallback callback) {
  DoShutDown(/*is_expected=*/true);
  std::move(callback).Run();
}

void SharingImpl::DoShutDown(bool is_expected) {
  nearby::NearbySharedRemotes::SetInstance(nullptr);

  if (!nearby_connections_ && !nearby_presence_ && !nearby_decoder_) {
    return;
  }

  nearby_connections_.reset();
  nearby_presence_.reset();
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

  if (deps->nearby_presence_credential_storage) {
    nearby_shared_remotes_->nearby_presence_credential_storage.Bind(
        std::move(deps->nearby_presence_credential_storage), io_task_runner_);
    nearby_shared_remotes_->nearby_presence_credential_storage
        .set_disconnect_handler(
            base::BindOnce(
                &SharingImpl::OnDisconnect, weak_ptr_factory_.GetWeakPtr(),
                MojoDependencyName::kNearbyPresenceCredentialStorage),
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

    if (deps->wifilan_dependencies->mdns_manager) {
      nearby_shared_remotes_->mdns_manager.Bind(
          std::move(deps->wifilan_dependencies->mdns_manager), io_task_runner_);
      nearby_shared_remotes_->mdns_manager.set_disconnect_handler(
          base::BindOnce(&SharingImpl::OnDisconnect,
                         weak_ptr_factory_.GetWeakPtr(),
                         MojoDependencyName::kMdnsManager),
          base::SequencedTaskRunner::GetCurrentDefault());
    }
  }

  if (deps->wifidirect_dependencies) {
    nearby_shared_remotes_->wifi_direct_firewall_hole_factory.Bind(
        std::move(deps->wifidirect_dependencies->firewall_hole_factory),
        io_task_runner_);
    nearby_shared_remotes_->wifi_direct_firewall_hole_factory
        .set_disconnect_handler(
            base::BindOnce(&SharingImpl::OnDisconnect,
                           weak_ptr_factory_.GetWeakPtr(),
                           MojoDependencyName::kFirewallHoleFactory),
            base::SequencedTaskRunner::GetCurrentDefault());

    nearby_shared_remotes_->wifi_direct_manager.Bind(
        std::move(deps->wifidirect_dependencies->wifi_direct_manager),
        io_task_runner_);
    nearby_shared_remotes_->wifi_direct_manager.set_disconnect_handler(
        base::BindOnce(&SharingImpl::OnDisconnect,
                       weak_ptr_factory_.GetWeakPtr(),
                       MojoDependencyName::kWifiDirectManager),
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
    case MojoDependencyName::kNearbyPresence:
      return "Nearby Presence";
    case MojoDependencyName::kNearbyShareDecoder:
      return "Decoder";
    case MojoDependencyName::kQuickStartDecoder:
      return "Quick Start Decoder";
    case MojoDependencyName::kNearbyPresenceCredentialStorage:
      return "Nearby Presence Credential Storage";
    case MojoDependencyName::kWifiDirectManager:
      return "WiFi Direct Manager";
    case MojoDependencyName::kMdnsManager:
      return "mDNS Manager";
  }
}

}  // namespace sharing
