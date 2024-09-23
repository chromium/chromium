// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_SHARING_IMPL_H_
#define CHROME_SERVICES_SHARING_SHARING_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/sharing/nearby/nearby_shared_remotes.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/mdns_responder.mojom-forward.h"
#include "services/network/public/mojom/p2p.mojom-forward.h"

namespace nearby::connections {
class NearbyConnections;
}  // namespace nearby::connections

namespace ash::nearby::presence {
class NearbyPresence;
}  // namespace ash::nearby::presence

namespace sharing {

class NearbySharingDecoder;

class SharingImpl : public mojom::Sharing {
 public:
  using NearbyConnectionsMojom = nearby::connections::mojom::NearbyConnections;
  using NearbyConnections = nearby::connections::NearbyConnections;
  using NearbyPresenceMojom = ash::nearby::presence::mojom::NearbyPresence;
  using NearbyPresence = ash::nearby::presence::NearbyPresence;
  using NearbyDependenciesPtr = ::sharing::mojom::NearbyDependenciesPtr;

  SharingImpl(mojo::PendingReceiver<mojom::Sharing> receiver,
              scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  SharingImpl(const SharingImpl&) = delete;
  SharingImpl& operator=(const SharingImpl&) = delete;
  ~SharingImpl() override;

  // mojom::Sharing:
  void Connect(
      NearbyDependenciesPtr deps,
      mojo::PendingReceiver<NearbyConnectionsMojom> connections_receiver,
      mojo::PendingReceiver<NearbyPresenceMojom> presence_receiver,
      mojo::PendingReceiver<::sharing::mojom::NearbySharingDecoder>
          decoder_receiver,
      mojo::PendingReceiver<ash::quick_start::mojom::QuickStartDecoder>
          quick_start_decoder_receiver) override;
  void ShutDown(ShutDownCallback callback) override;

 private:
  friend class SharingImplTest;

  // These values are used for metrics. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class MojoDependencyName {
    kNearbyConnections = 0,
    kBluetoothAdapter = 1,
    kSocketManager = 2,
    kMdnsResponder = 3,
    kIceConfigFetcher = 4,
    kWebRtcSignalingMessenger = 5,
    kCrosNetworkConfig = 6,
    kFirewallHoleFactory = 7,
    kTcpSocketFactory = 8,
    kNearbyPresence = 9,
    kNearbyShareDecoder = 10,
    kQuickStartDecoder = 11,
    kNearbyPresenceCredentialStorage = 12,
    kWifiDirectManager = 13,
    kMdnsManager = 14,
    kMaxValue = kMdnsManager
  };

  void DoShutDown(bool is_expected);
  void OnDisconnect(MojoDependencyName mojo_dependency_name);
  void InitializeNearbySharedRemotes(NearbyDependenciesPtr deps);
  std::string GetMojoDependencyName(MojoDependencyName dependency_name);

  mojo::Receiver<mojom::Sharing> receiver_;
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  std::unique_ptr<nearby::NearbySharedRemotes> nearby_shared_remotes_;

  std::unique_ptr<NearbyConnections> nearby_connections_;

  std::unique_ptr<NearbyPresence> nearby_presence_;

  std::unique_ptr<NearbySharingDecoder> nearby_decoder_;

  std::unique_ptr<ash::quick_start::mojom::QuickStartDecoder>
      quick_start_decoder_;

  base::WeakPtrFactory<SharingImpl> weak_ptr_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_SHARING_IMPL_H_
