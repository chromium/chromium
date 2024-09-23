// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/components/nearby/presence/enums/nearby_presence_enums.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/nearby/src/presence/presence_device.h"

namespace ash::nearby::presence {

class NearbyPresenceConnectionsManager;

// This service implements Nearby Presence on top of the Nearby Presence .mojom
// interface.
class NearbyPresenceService {
 public:
  using PresenceIdentityType = ash::nearby::presence::mojom::IdentityType;
  using PresenceFilter = ash::nearby::presence::mojom::PresenceScanFilter;

  NearbyPresenceService();
  virtual ~NearbyPresenceService();

  enum class Action {
    kActiveUnlock = 8,
    kNearbyShare = 9,
    kInstantTethering = 10,
    kPhoneHub = 11,
    kPresenceManager = 12,
    kFinder = 13,
    kFastPairSass = 14,
    kTapToTransfer = 15,
    kLast
  };

  struct ScanFilter {
    ScanFilter(::nearby::internal::IdentityType identity_type,
               const std::vector<Action>& actions);
    ScanFilter(const ScanFilter&);
    ~ScanFilter();

    ::nearby::internal::IdentityType identity_type_;
    std::vector<Action> actions_;
  };

  class ScanDelegate {
   public:
    ScanDelegate();
    virtual ~ScanDelegate();

    virtual void OnPresenceDeviceFound(
        ::nearby::presence::PresenceDevice presence_device) = 0;
    virtual void OnPresenceDeviceChanged(
        ::nearby::presence::PresenceDevice presence_device) = 0;
    virtual void OnPresenceDeviceLost(
        ::nearby::presence::PresenceDevice presence_device) = 0;
    virtual void OnScanSessionInvalidated() = 0;
  };

  class ScanSession {
   public:
    ScanSession(mojo::PendingRemote<ash::nearby::presence::mojom::ScanSession>
                    pending_remote,
                base::OnceClosure on_disconnect_callback);
    ~ScanSession();

   private:
    mojo::Remote<ash::nearby::presence::mojom::ScanSession> remote_;
    base::OnceClosure on_disconnect_callback_;
  };

  virtual void StartScan(
      ScanFilter scan_filter,
      ScanDelegate* scan_delegate,
      base::OnceCallback<void(std::unique_ptr<ScanSession>, enums::StatusCode)>
          on_start_scan_callback) = 0;

  virtual void Initialize(base::OnceClosure on_initialized_callback) = 0;

  // Triggers an immediate request to update Nearby Presence credentials, which
  // involves:
  //     1. Fetching the local device's credentials from the NP library and
  ///       uploading them to the NP server.
  //     2. Downloading remote devices' credentials from the NP server and
  //        saving them to the NP library.
  virtual void UpdateCredentials() = 0;

  virtual std::unique_ptr<NearbyPresenceConnectionsManager>
  CreateNearbyPresenceConnectionsManager() = 0;
};

// TODO(b/342473553): Migrate this function and implementation to
// //chromeos/ash/components/nearby/presence/enums.
std::ostream& operator<<(std::ostream& stream,
                         const enums::StatusCode status_code);

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_
