// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_PROCESS_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_PROCESS_MANAGER_H_

#include <memory>

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash {
namespace nearby {

// Manages the life cycle of the Nearby utility process, which hosts
// functionality for both Nearby Connections and Nearby Share.
class NearbyProcessManager : public KeyedService {
 public:
  class NearbyProcessReference {
   public:
    virtual ~NearbyProcessReference() = default;
    virtual const mojo::SharedRemote<
        ::nearby::connections::mojom::NearbyConnections>&
    GetNearbyConnections() const = 0;
    virtual const mojo::SharedRemote<
        ::ash::nearby::presence::mojom::NearbyPresence>&
    GetNearbyPresence() const = 0;
    virtual const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&
    GetNearbySharingDecoder() const = 0;
    virtual const mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>&
    GetQuickStartDecoder() const = 0;
  };

  // These values are used for metrics. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated. Keep in sync with the
  // `NearbyConnectionsUtilityProcessShutdownReason` enum found at
  // //tools/metrics/histograms/metadata/nearby/enums.xml
  enum class NearbyProcessShutdownReason {
    kNormal = 0,
    kCrash = 1,
    kDecoderMojoPipeDisconnection = 3,
    kConnectionsMojoPipeDisconnection = 4,
    kPresenceMojoPipeDisconnection = 5,
    kMaxValue = kPresenceMojoPipeDisconnection
  };

  using NearbyProcessStoppedCallback =
      base::OnceCallback<void(NearbyProcessShutdownReason)>;

  ~NearbyProcessManager() override = default;

  // Returns a reference which allows clients invoke functions implemented by
  // the Nearby utility process. If at least one NearbyProcessReference is held,
  // NearbyProcessManager attempts to keep the Nearby utility process alive.
  //
  // Note that it is possible that the Nearby process could crash and shut down
  // while a NearbyReference is still held; if this occurs,
  // |on_process_stopped_callback| will be invoked, and the client should no
  // longer use the invalidated NearbyReference.
  //
  // Clients should delete their NearbyProcessReference object when they are no
  // longer using it; when there are no remaining NearbyProcessReference
  // objects, NearbyProcessManager shuts down the utility process. Note that
  // once clients delete the returned NearbyProcessReference, they will no
  // longer receive a callback once the process has stopped.
  //
  // Note: This function returns null if the user session is initializing or
  // shutting down.
  virtual std::unique_ptr<NearbyProcessReference> GetNearbyProcessReference(
      NearbyProcessStoppedCallback on_process_stopped_callback) = 0;

  // Immediately shut down the utility process, bypassing any debounce logic.
  virtual void ShutDownProcess() = 0;

 private:
  using KeyedService::Shutdown;
};

std::ostream& operator<<(
    std::ostream& os,
    const NearbyProcessManager::NearbyProcessShutdownReason& reason);

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_PROCESS_MANAGER_H_
