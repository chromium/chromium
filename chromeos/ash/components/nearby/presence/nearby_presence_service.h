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
  using PresenceStatus = ash::nearby::presence::mojom::StatusCode;
  using PresenceIdentityType = ash::nearby::presence::mojom::IdentityType;
  using PresenceFilter = ash::nearby::presence::mojom::PresenceScanFilter;

  NearbyPresenceService();
  virtual ~NearbyPresenceService();

  enum class IdentityType { kPrivate };

  // TODO(b/276642472): Include real NearbyPresence ActionType.
  enum class ActionType {
    action_1,
    action_2,
  };

  // TODO(b/276642472): Move PresenceDevice into its own class and file, to
  // inherit from the upcoming Nearby Connections Device class.
  class PresenceDevice {
   public:
    PresenceDevice(::nearby::internal::DeviceType device_type,
                   absl::optional<std::string> stable_device_id,
                   std::string endpoint_id,
                   std::string device_name,
                   std::vector<ActionType> actions,
                   int rssi);
    PresenceDevice(const PresenceDevice&) = delete;
    PresenceDevice& operator=(const PresenceDevice&) = delete;
    ~PresenceDevice();

    ::nearby::internal::DeviceType GetType() const { return device_type_; }

    const absl::optional<std::string> GetStableId() const {
      return stable_device_id_;
    }
    const std::string& GetEndpointId() const { return endpoint_id_; }
    const std::string& GetName() const { return device_name_; }
    const std::vector<ActionType> GetActions() const { return actions_; }
    int GetRssi() const { return rssi_; }

   private:
    ::nearby::internal::DeviceType device_type_;
    absl::optional<std::string> stable_device_id_;
    std::string endpoint_id_;
    std::string device_name_;
    std::vector<ActionType> actions_;
    int rssi_;
  };

  struct ScanFilter {
    ScanFilter(IdentityType identity_type,
               const std::vector<ActionType>& actions);
    ScanFilter(const ScanFilter&);
    ~ScanFilter();

    IdentityType identity_type_;
    std::vector<ActionType> actions_;
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
      base::OnceCallback<void(std::unique_ptr<ScanSession>, PresenceStatus)>
          on_start_scan_callback) = 0;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_
