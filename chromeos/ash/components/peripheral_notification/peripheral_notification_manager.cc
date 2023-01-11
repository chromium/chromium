// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"

namespace ash {

namespace {

PeripheralNotificationManager* g_instance = nullptr;

const int kBillboardDeviceClassCode = 17;
constexpr char thunderbolt_file_path[] = "/sys/bus/thunderbolt/devices/0-0";

void RecordConnectivityMetric(
    PeripheralNotificationManager::PeripheralConnectivityResults results) {
  base::UmaHistogramEnumeration("Ash.Peripheral.ConnectivityResults", results);
}

// Checks if the board supports Thunderbolt.
bool CheckIfThunderboltFilepathExists(std::string root_prefix) {
  return base::PathExists(base::FilePath(root_prefix + thunderbolt_file_path));
}

}  // namespace

PeripheralNotificationManager::PeripheralNotificationManager(
    bool is_guest_profile,
    bool is_pcie_tunneling_allowed)
    : is_guest_profile_(is_guest_profile),
      is_pcie_tunneling_allowed_(is_pcie_tunneling_allowed) {
  DCHECK(TypecdClient::Get());
  DCHECK(PciguardClient::Get());
  TypecdClient::Get()->AddObserver(this);
  PciguardClient::Get()->AddObserver(this);
}

PeripheralNotificationManager::~PeripheralNotificationManager() {
  TypecdClient::Get()->RemoveObserver(this);
  PciguardClient::Get()->RemoveObserver(this);

  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void PeripheralNotificationManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PeripheralNotificationManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PeripheralNotificationManager::
    NotifyLimitedPerformancePeripheralReceived() {
  // Show no notifications if PCIe tunneling is allowed.
  if (is_pcie_tunneling_allowed_)
    return;

  for (auto& observer : observer_list_)
    observer.OnLimitedPerformancePeripheralReceived();
}

void PeripheralNotificationManager::NotifyGuestModeNotificationReceived(
    bool is_thunderbolt_only) {
  for (auto& observer : observer_list_)
    observer.OnGuestModeNotificationReceived(is_thunderbolt_only);
}

void PeripheralNotificationManager::NotifyPeripheralBlockedReceived() {
  for (auto& observer : observer_list_)
    observer.OnPeripheralBlockedReceived();
}

void PeripheralNotificationManager::OnBillboardDeviceConnected(
    bool billboard_is_supported) {
  if (!features::IsPcieBillboardNotificationEnabled()) {
    return;
  }

  if (!billboard_is_supported) {
    for (auto& observer : observer_list_)
      observer.OnBillboardDeviceConnected();

    RecordConnectivityMetric(PeripheralConnectivityResults::kBillboardDevice);
  }
}

void PeripheralNotificationManager::OnThunderboltDeviceConnected(
    bool is_thunderbolt_only) {
  if (is_guest_profile_) {
    NotifyGuestModeNotificationReceived(is_thunderbolt_only);
    RecordConnectivityMetric(
        is_thunderbolt_only
            ? PeripheralConnectivityResults::kTBTOnlyAndBlockedInGuestSession
            : PeripheralConnectivityResults::kAltModeFallbackInGuestSession);
    return;
  }

  // Only show notifications if the peripheral is operating at limited
  // performance.
  if (!is_pcie_tunneling_allowed_) {
    NotifyLimitedPerformancePeripheralReceived();
    RecordConnectivityMetric(
        is_thunderbolt_only
            ? PeripheralConnectivityResults::kTBTOnlyAndBlockedByPciguard
            : PeripheralConnectivityResults::kAltModeFallbackDueToPciguard);
    return;
  }

  RecordConnectivityMetric(
      PeripheralConnectivityResults::kTBTSupportedAndAllowed);
}

void PeripheralNotificationManager::OnBlockedThunderboltDeviceConnected(
    const std::string& name) {
  // Currently the device name is not shown in the notification.
  NotifyPeripheralBlockedReceived();
  RecordConnectivityMetric(PeripheralConnectivityResults::kPeripheralBlocked);
}

void PeripheralNotificationManager::OnCableWarning(
    typecd::CableWarningType cable_warning_type) {
  // Decode cable warnging signal.
  switch (cable_warning_type) {
    case typecd::CableWarningType::kInvalidDpCable:
      NotifyInvalidDpCable();
      RecordConnectivityMetric(PeripheralConnectivityResults::kInvalidDpCable);
      break;
    case typecd::CableWarningType::kInvalidUSB4ValidTBTCable:
      NotifyInvalidUSB4ValidTBTCableWarning();
      RecordConnectivityMetric(
          PeripheralConnectivityResults::kInvalidUSB4ValidTBTCable);
      break;
    case typecd::CableWarningType::kInvalidUSB4Cable:
      NotifyInvalidUSB4CableWarning();
      RecordConnectivityMetric(
          PeripheralConnectivityResults::kInvalidUSB4Cable);
      break;
    case typecd::CableWarningType::kInvalidTBTCable:
      NotifyInvalidTBTCableWarning();
      RecordConnectivityMetric(PeripheralConnectivityResults::kInvalidTBTCable);
      break;
    case typecd::CableWarningType::kSpeedLimitingCable:
      NotifySpeedLimitingCableWarning();
      RecordConnectivityMetric(
          PeripheralConnectivityResults::kSpeedLimitingCable);
      break;
    default:
      break;
  }
}

void PeripheralNotificationManager::NotifyInvalidDpCable() {
  for (auto& observer : observer_list_)
    observer.OnInvalidDpCableWarning();
}

void PeripheralNotificationManager::NotifyInvalidUSB4ValidTBTCableWarning() {
  for (auto& observer : observer_list_)
    observer.OnInvalidUSB4ValidTBTCableWarning();
}

void PeripheralNotificationManager::NotifyInvalidUSB4CableWarning() {
  for (auto& observer : observer_list_)
    observer.OnInvalidUSB4CableWarning();
}

void PeripheralNotificationManager::NotifyInvalidTBTCableWarning() {
  for (auto& observer : observer_list_)
    observer.OnInvalidTBTCableWarning();
}

void PeripheralNotificationManager::NotifySpeedLimitingCableWarning() {
  for (auto& observer : observer_list_)
    observer.OnSpeedLimitingCableWarning();
}

void PeripheralNotificationManager::OnDeviceConnected(
    device::mojom::UsbDeviceInfo* device) {
  if (device->class_code == kBillboardDeviceClassCode) {
    // PathExist is a blocking call. PostTask it and wait on the result.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&CheckIfThunderboltFilepathExists, root_prefix_),
        base::BindOnce(
            &PeripheralNotificationManager::OnBillboardDeviceConnected,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void PeripheralNotificationManager::SetPcieTunnelingAllowedState(
    bool is_pcie_tunneling_allowed) {
  is_pcie_tunneling_allowed_ = is_pcie_tunneling_allowed;
}

void PeripheralNotificationManager::SetRootPrefixForTesting(
    const std::string& prefix) {
  root_prefix_ = prefix;
}

// static
void PeripheralNotificationManager::Initialize(bool is_guest_profile,
                                               bool is_pcie_tunneling_allowed) {
  CHECK(!g_instance);
  g_instance = new PeripheralNotificationManager(is_guest_profile,
                                                 is_pcie_tunneling_allowed);
}

// static
void PeripheralNotificationManager::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = NULL;
}

// static
PeripheralNotificationManager* PeripheralNotificationManager::Get() {
  CHECK(g_instance);
  return g_instance;
}

// static
bool PeripheralNotificationManager::IsInitialized() {
  return g_instance;
}

}  // namespace ash
