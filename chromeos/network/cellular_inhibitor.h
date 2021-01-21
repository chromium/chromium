// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_INHIBITOR_H_
#define CHROMEOS_NETWORK_CELLULAR_INHIBITOR_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace chromeos {

class DeviceState;
class NetworkStateHandler;
class NetworkDeviceHandler;

// Updates the "Inhibited" property of the Cellular device.
//
// When some SIM-related operations are performed, properties of the Cellular
// device can change to a temporary value and then change back. To prevent churn
// in these properties, Shill provides the "Inhibited" property to inhibit any
// scans.
//
// This class is intended to be used when performing such actions to ensure that
// these transient states never occur.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularInhibitor {
 public:
  CellularInhibitor();
  CellularInhibitor(const CellularInhibitor&) = delete;
  CellularInhibitor& operator=(const CellularInhibitor&) = delete;
  ~CellularInhibitor();

  void Init(NetworkStateHandler* network_state_handler,
            NetworkDeviceHandler* network_device_handler);

  // Callback which returns true upon success or false upon failure.
  using SuccessCallback = base::OnceCallback<void(bool)>;

  // Sets or unsets the Inhibited property of the Cellular device.
  void InhibitCellularScanning(SuccessCallback callback);
  void UninhibitCellularScanning(SuccessCallback callback);

 private:
  const DeviceState* GetCellularDevice() const;

  void SetInhibitProperty(bool new_inhibit_value, SuccessCallback callback);
  void OnSetPropertySuccess(
      const base::RepeatingCallback<void(bool)>& success_callback);
  void OnSetPropertyError(
      const base::RepeatingCallback<void(bool)>& success_callback,
      bool attempted_inhibit,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkDeviceHandler* network_device_handler_ = nullptr;

  base::WeakPtrFactory<CellularInhibitor> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_INHIBITOR_H_
