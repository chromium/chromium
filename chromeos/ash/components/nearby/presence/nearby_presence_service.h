// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/nearby/internal/proto/metadata.pb.h"

namespace ash::nearby::presence {

// This service implements Nearby Presence on top of the Nearby Presence .mojom
// interface.
class NearbyPresenceService {
 public:
  using PresenceIdentityType = ash::nearby::presence::mojom::IdentityType;
  using PresenceFilter = ash::nearby::presence::mojom::PresenceScanFilter;

  NearbyPresenceService();
  virtual ~NearbyPresenceService();

  enum class IdentityType {
    kUnspecified,
    kPrivate,
    kTrusted,
    kPublic,
    kProvisioned
  };

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

  // This is a super set of the absl status code found in
  // //mojo/public/mojom/base/absl_status.mojom with the only difference being
  // the addition of kFailedToStartProcess. Any updates to absl_status should be
  // reflected here.
  enum class StatusCode {
    kAbslOk = 0,
    kAbslCancelled = 1,
    kAbslUnknown = 2,
    kAbslInvalidArgument = 3,
    kAbslDeadlineExceeded = 4,
    kAbslNotFound = 5,
    kAbslAlreadyExists = 6,
    kAbslPermissionDenied = 7,
    kAbslResourceExhausted = 8,
    kAbslFailedPrecondition = 9,
    kAbslAborted = 10,
    kAbslOutOfRange = 11,
    kAbslUnimplemented = 12,
    kAbslInternal = 13,
    kAbslUnavailable = 14,
    kAbslDataLoss = 15,
    kAbslUnauthenticated = 16,
    kFailedToStartProcess = 17,
  };

  // TODO(b/276642472): Move PresenceDevice into its own class and file, to
  // inherit from the upcoming Nearby Connections Device class.
  class PresenceDevice {
   public:
    PresenceDevice(::nearby::internal::Metadata metadata,
                   absl::optional<std::string> stable_device_id,
                   std::string endpoint_id,
                   std::vector<Action> actions,
                   int rssi);

    PresenceDevice(const PresenceDevice&) = delete;
    PresenceDevice& operator=(const PresenceDevice&) = delete;
    ~PresenceDevice();

    ::nearby::internal::DeviceType GetType() const {
      return metadata_.device_type();
    }

    const absl::optional<std::string> GetStableId() const {
      return stable_device_id_;
    }
    const std::string& GetEndpointId() const { return endpoint_id_; }
    const std::string& GetName() const { return metadata_.device_name(); }
    const std::vector<Action> GetActions() const { return actions_; }
    int GetRssi() const { return rssi_; }

   private:
    ::nearby::internal::Metadata metadata_;
    absl::optional<std::string> stable_device_id_;
    std::string endpoint_id_;
    std::vector<Action> actions_;
    int rssi_;
  };

  struct ScanFilter {
    ScanFilter(IdentityType identity_type, const std::vector<Action>& actions);
    ScanFilter(const ScanFilter&);
    ~ScanFilter();

    IdentityType identity_type_;
    std::vector<Action> actions_;
  };

  class ScanDelegate {
   public:
    ScanDelegate();
    virtual ~ScanDelegate();

    virtual void OnPresenceDeviceFound(
        const PresenceDevice& presence_device) = 0;
    virtual void OnPresenceDeviceChanged(
        const PresenceDevice& presence_device) = 0;
    virtual void OnPresenceDeviceLost(
        const PresenceDevice& presence_device) = 0;
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
      base::OnceCallback<void(std::unique_ptr<ScanSession>, StatusCode)>
          on_start_scan_callback) = 0;

  virtual void Initialize(base::OnceClosure on_initialized_callback) = 0;

  // Triggers an immediate request to update Nearby Presence credentials, which
  // involves:
  //     1. Fetching the local device's credentials from the NP library and
  ///       uploading them to the NP server.
  //     2. Downloading remote devices' credentials from the NP server and
  //        saving them to the NP library.
  virtual void UpdateCredentials() = 0;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_
