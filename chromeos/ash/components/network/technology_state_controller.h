// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TECHNOLOGY_STATE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TECHNOLOGY_STATE_CONTROLLER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_type_pattern.h"

namespace ash {

class NetworkStateHandler;

// This class serves as the primary entry point for enabling and disabling
// network devices and technologies, and ensures handling of hotspot/Wifi
// concurrency issues by disabling one before enabling the other.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) TechnologyStateController {
 public:
  class HotspotOperationDelegate {
   public:
    virtual ~HotspotOperationDelegate() = default;

    // Prepare for enable Wifi technology by disabling hotspot if active.
    // Calls |callback| when the preparation is completed.
    virtual void PrepareEnableWifi(
        base::OnceCallback<void(bool prepare_success)> callback) = 0;
  };

  // Constant for |error_name| from network_handler::ErrorCallback. This error
  // name indicated failed to disable hotspot before enable Wifi.
  static const char kErrorDisableHotspot[];

  TechnologyStateController();
  TechnologyStateController(const TechnologyStateController&) = delete;
  TechnologyStateController& operator=(const TechnologyStateController&) =
      delete;
  ~TechnologyStateController();

  void Init(NetworkStateHandler* network_state_handler);

  // Asynchronously sets the technologies enabled property for |type|.Calls
  // |error_callback| upon failure.
  void SetTechnologiesEnabled(const NetworkTypePattern& type,
                              bool enabled,
                              network_handler::ErrorCallback error_callback);

  // Callback for the PrepareEnableHotspot method. |wifi_turned_off| indicates
  // whether Wifi technology is turned off during the preparation.
  // |prepare_success| indicates whether the preparation completes successfully.
  using PrepareEnableHotspotCallback =
      base::OnceCallback<void(bool prepare_success, bool wifi_turned_off)>;

  // Prepare for enable hotspot by disabling Wifi technology if active. Calls
  // |callback| when the preparation is completed.
  void PrepareEnableHotspot(PrepareEnableHotspotCallback callback);

  void set_hotspot_operation_delegate(
      HotspotOperationDelegate* hotspot_operation_delegate) {
    hotspot_operation_delegate_ = hotspot_operation_delegate;
  }

 private:
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<HotspotOperationDelegate> hotspot_operation_delegate_ = nullptr;

  void OnDisableWifiForHotspotFailed(PrepareEnableHotspotCallback callback,
                                     const std::string& error_name);
  void OnPrepareEnableWifiCompleted(
      const NetworkTypePattern& type,
      network_handler::ErrorCallback error_callback,
      bool success);

  base::WeakPtrFactory<TechnologyStateController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TECHNOLOGY_STATE_CONTROLLER_H_
