// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/cross_device/nearby/nearby_features.h"

namespace nearby::chrome {

BluetoothAdapter::BluetoothAdapter(
    const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter)
    : adapter_(adapter) {
  DCHECK(adapter_.is_bound());
}

BluetoothAdapter::~BluetoothAdapter() = default;

bool BluetoothAdapter::SetStatus(Status status) {
  // TODO(b/154848416): Implement this method.
  NOTIMPLEMENTED();
  return true;
}

bool BluetoothAdapter::IsEnabled() const {
  bluetooth::mojom::AdapterInfoPtr info;
  bool success = adapter_->GetInfo(&info);
  return success && info->present && info->powered;
}

BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  bluetooth::mojom::AdapterInfoPtr info;
  bool success = adapter_->GetInfo(&info);

  if (!success || !info->present)
    return ScanMode::kUnknown;
  else if (!info->powered)
    return ScanMode::kNone;
  else if (!info->discoverable)
    return ScanMode::kConnectable;

  return ScanMode::kConnectableDiscoverable;
}

bool BluetoothAdapter::SetScanMode(BluetoothAdapter::ScanMode scan_mode) {
  // This method is only used to trigger discoverability -- so there is no
  // difference between passing ScanMode::kUnknown, ScanMode::kNone, or
  // ScanMode::kConnectable -- they will all turn off discoverability.

  bool set_discoverable_success = false;
  bool call_success =
      adapter_->SetDiscoverable(scan_mode == ScanMode::kConnectableDiscoverable,
                                &set_discoverable_success);

  bool success = call_success && set_discoverable_success;
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.Adapter.SetScanMode.Result", success);

  return success;
}

std::string BluetoothAdapter::GetName() const {
  bluetooth::mojom::AdapterInfoPtr info;
  bool success = adapter_->GetInfo(&info);
  return success ? info->name : std::string();
}

bool BluetoothAdapter::SetName(std::string_view name) {
  return SetName(name, /*persist=*/true);
}

bool BluetoothAdapter::SetName(std::string_view name, bool persist) {
  // The `persist` parameter is ignored by ChromeOS; we always persist the
  // requested adapter name change. The `persist` argument only exists to
  // support Windows. See b/234135746 for more context."

  if (!features::IsNearbyBluetoothClassicAdvertisingEnabled()) {
    // SetName is called in Nearby Connections to use the name for
    // "advertising", triggered by Nearby Connections becoming discoverable over
    // Bluetooth Classic. We return true here despite ignoring the request to
    // change the adapter name. This flag should only be false in tests, in
    // order to ensure that Classic "advertising" is not active.
    VLOG(1) << ": Classic advertising disabled, ignoring SetName for name: "
            << name.data();
    return true;
  }

  bool set_name_success = false;
  bool call_success = adapter_->SetName(name.data(), &set_name_success);

  bool success = call_success && set_name_success;
  base::UmaHistogramBoolean(
      "Nearby.Connections.Bluetooth.Adapter.SetName.Result", success);

  return success;
}

std::string BluetoothAdapter::GetMacAddress() const {
  bluetooth::mojom::AdapterInfoPtr info;
  bool success = adapter_->GetInfo(&info);
  return success ? info->address : std::string();
}

std::string BluetoothAdapter::GetAddress() const {
  return GetMacAddress();
}

BluetoothAdapter::UniqueId BluetoothAdapter::GetUniqueId() const {
  // The unique id is not used by ChromeOS and this remains unimplemented. If
  // functionality is needed later on, this can be implemented.
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace nearby::chrome
