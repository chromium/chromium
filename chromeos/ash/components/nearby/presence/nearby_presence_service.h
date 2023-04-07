// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

namespace ash::nearby::presence {

// This service implements Nearby Presence on top of the Nearby Presence .mojom
// interface.
class NearbyPresenceService {
 public:
  NearbyPresenceService();
  virtual ~NearbyPresenceService();

  enum class IdentityType { kPrivate };

  // TODO(b/276642472): Include real NearbyPresence ActionType.
  enum class ActionType {
    action_1,
    action_2,
  };

  struct ScanSession {
    std::string session_name;
  };

  // TODO(b/276642472): Move PresenceDevice into its own class and file, to
  // inherit from the upcoming Nearby Connections Device class.
  class PresenceDevice {
   public:
    enum class DeviceType {
      kUnspecified,
      kPhone,
      kTablet,
      kDisplay,
      kChromeOS,
      kTv,
      kWatch
    };

    PresenceDevice(DeviceType device_type,
                   std::string stable_device_id,
                   std::string device_name,
                   std::vector<ActionType> actions,
                   int rssi);
    PresenceDevice(const PresenceDevice&) = delete;
    PresenceDevice& operator=(const PresenceDevice&) = delete;
    ~PresenceDevice();

   private:
    DeviceType device_type_;
    std::string stable_device_id_;
    std::string device_name_;
    std::vector<ActionType> actions_;
    int rssi_;
  };

  struct ScanFilter {
    ScanFilter();
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
  };

  virtual std::unique_ptr<ScanSession> StartScan(
      ScanFilter scan_filter,
      ScanDelegate* scan_delegate) = 0;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_NEARBY_PRESENCE_SERVICE_H_
