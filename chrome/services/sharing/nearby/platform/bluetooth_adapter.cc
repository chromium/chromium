// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"
#include "base/metrics/histogram_functions.h"

namespace nearby {
namespace chrome {

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

bool BluetoothAdapter::SetName(absl::string_view name) {
  return SetName(name, /*persist=*/true);
}

bool BluetoothAdapter::SetName(absl::string_view name, bool persist) {
  // The persist parameter is not used by ChromeOS. The function was created
  // in the base class to support Windows. For ChromeOS, we will always pass
  // true. If this capability is needed later on, the reference can be found
  // at b/234135746.
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

}  // namespace chrome
}  // namespace nearby
