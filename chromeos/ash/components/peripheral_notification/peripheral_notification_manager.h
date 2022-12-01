// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PERIPHERAL_NOTIFICATION_PERIPHERAL_NOTIFICATION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PERIPHERAL_NOTIFICATION_PERIPHERAL_NOTIFICATION_MANAGER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/pciguard/pciguard_client.h"
#include "chromeos/ash/components/dbus/typecd/typecd_client.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"

namespace device {
namespace mojom {
class UsbDeviceInfo;
}  // namespace mojom
}  // namespace device

namespace ash {

// This class is responsible for listening to TypeCd and Pciguard D-Bus calls
// and translating those signals to notification observer events. It handles
// additional logic such determining if notifications are required or whether a
// guest-session notification is needed.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_PERIPHERAL_NOTIFICATION)
    PeripheralNotificationManager : public TypecdClient::Observer,
                                    public PciguardClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called to notify observers, primarily notification controllers, that a
    // recently plugged in Thunderbolt/USB4 device is running at limited
    // performance. This can be called multiple times.
    virtual void OnLimitedPerformancePeripheralReceived() = 0;

    // Called to notify observers, primarily notification controllers, that a
    // Thunderbolt/USB4 device has been plugged in during a guest session. Can
    // be called multiple times.
    virtual void OnGuestModeNotificationReceived(bool is_thunderbolt_only) = 0;

    // Called to notify observers, primarily notification controllers, that the
    // recently plugged in Thunderbolt/USB4 device is in the block list. The
    // block list is specified by the Pciguard Daemon.
    virtual void OnPeripheralBlockedReceived() = 0;

    // Called to notify observers, primarily notification controllers, that the
    // recently plugged in Thunderbolt/USB4 device is a billboard device that is
    // not supported by the board.
    virtual void OnBillboardDeviceConnected() = 0;

    // Called to notify user of possibly invalid dp cable. This signal will be
    // sent by typecd when the partner meets the conditions for DP alternate
    // mode, but the cable does not.
    virtual void OnInvalidDpCableWarning() = 0;

    // Called to notify the user that their USB4 device is unable to establish a
    // USB4 connection because of the cable. In this case, the connection will
    // fall back to Thunderbolt.
    virtual void OnInvalidUSB4ValidTBTCableWarning() = 0;

    // Called to notify the user that their USB4 device is unable to establish a
    // USB4 connection because of the cable. It is similar to
    // OnUSB4ToThundeboltCableWarning, but in this case the connection will
    // fall back to DisplayPort, USB 3.2 or USB 2.0.
    virtual void OnInvalidUSB4CableWarning() = 0;

    // Called to notify the user that their Thubderbolt device is unable to
    // establish a Thunderbolt connection, and will instead use DisplayPort,
    // USB 3.2 or USB 2.0.
    virtual void OnInvalidTBTCableWarning() = 0;

    // Called to notify the user when their 40 Gbps USB4 device is unable to use
    // 40 Gbps data transmission because of the cable. Transmissions speeds will
    // decrease to 20 Gbps, 10 Gbps or 5 Gbps.
    virtual void OnSpeedLimitingCableWarning() = 0;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PeripheralConnectivityResults {
    kTBTSupportedAndAllowed = 0,
    kTBTOnlyAndBlockedByPciguard = 1,
    kTBTOnlyAndBlockedInGuestSession = 2,
    kAltModeFallbackDueToPciguard = 3,
    kAltModeFallbackInGuestSession = 4,
    kPeripheralBlocked = 5,
    kBillboardDevice = 6,
    kInvalidDpCable = 7,
    kInvalidUSB4ValidTBTCable = 8,
    kInvalidUSB4Cable = 9,
    kInvalidTBTCable = 10,
    kSpeedLimitingCable = 11,
    kMaxValue = kSpeedLimitingCable,
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(bool is_guest_profile, bool is_pcie_tunneling_allowed);

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance pointer.
  static PeripheralNotificationManager* Get();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  void SetPcieTunnelingAllowedState(bool is_pcie_tunneling_allowed);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnDeviceConnected(device::mojom::UsbDeviceInfo* device);

 private:
  friend class PeripheralNotificationManagerTest;

  PeripheralNotificationManager(bool is_guest_profile,
                                bool is_pcie_tunneling_allowed);
  PeripheralNotificationManager(const PeripheralNotificationManager&) = delete;
  PeripheralNotificationManager& operator=(
      const PeripheralNotificationManager&) = delete;
  ~PeripheralNotificationManager() override;

  // TypecdClient::Observer:
  void OnThunderboltDeviceConnected(bool is_thunderbolt_only) override;
  void OnCableWarning(typecd::CableWarningType cable_warning_type) override;

  // PciguardClient::Observer:
  void OnBlockedThunderboltDeviceConnected(
      const std::string& device_name) override;

  // Call to notify observers that a new notification is needed.
  void NotifyLimitedPerformancePeripheralReceived();
  void NotifyGuestModeNotificationReceived(bool is_thunderbolt_only);
  void NotifyPeripheralBlockedReceived();
  void OnBillboardDeviceConnected(bool billboard_is_supported);
  void NotifyInvalidDpCable();
  void NotifyInvalidUSB4ValidTBTCableWarning();
  void NotifyInvalidUSB4CableWarning();
  void NotifyInvalidTBTCableWarning();
  void NotifySpeedLimitingCableWarning();

  // Called by unit tests to set up root_prefix_ for simulating the existence
  // of a system folder.
  void SetRootPrefixForTesting(const std::string& prefix);

  const bool is_guest_profile_;
  // Pcie tunneling refers to allowing Thunderbolt/USB4 peripherals to run at
  // full capacity by utilizing the PciExpress protocol. If this is set to
  // false, we anticipate that the plugged in Thunderbolt/USB4 periphal is
  // operating at either Alt-mode (i.e. fallback to an older protocol) or
  // in a restricted state (e.g. certain devices are Thunderbolt only).
  bool is_pcie_tunneling_allowed_;
  base::ObserverList<Observer> observer_list_;

  std::string root_prefix_ = "";

  // Used for callbacks.
  base::WeakPtrFactory<PeripheralNotificationManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PERIPHERAL_NOTIFICATION_PERIPHERAL_NOTIFICATION_MANAGER_H_
