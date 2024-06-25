// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-forward.h"

namespace ash {

class HotspotFeatureUsageMetrics;

// Handles enable or disable hotspot.
//
// Enabling the hotspot involves the following operations:
// 1. Check hotspot capabilities
// 2. Check tethering readiness
// 3. Enable tethering from Shill
//
// Enable or disable requests executed as they come in but are ignored if there
// is already a pending request.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotController
    : public TechnologyStateController::HotspotOperationDelegate,
      public HotspotStateHandler::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnHotspotTurnedOn() = 0;

    virtual void OnHotspotTurnedOff(
        hotspot_config::mojom::DisableReason disable_reason) = 0;
  };
  HotspotController();
  HotspotController(const HotspotController&) = delete;
  HotspotController& operator=(const HotspotController&) = delete;
  ~HotspotController() override;

  void Init(HotspotCapabilitiesProvider* hotspot_capabilities_provider,
            HotspotFeatureUsageMetrics* hotspot_feature_usage_metrics,
            HotspotStateHandler* hotspot_state_handler,
            TechnologyStateController* technolog_state_controller);

  // Return callback for the EnableHotspot or DisableHotspot method.
  using HotspotControlCallback = base::OnceCallback<void(
      hotspot_config::mojom::HotspotControlResult control_result)>;

  // Checks if there is an existing request and if there isn't one, proceeds to
  // execute it.
  void EnableHotspot(HotspotControlCallback callback);
  void DisableHotspot(HotspotControlCallback callback,
                      hotspot_config::mojom::DisableReason disable_reason);

  // Set whether Hotspot should be allowed/disallowed by policy.
  void SetPolicyAllowHotspot(bool allow_hotspot);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  friend class HotspotControllerTest;
  friend class HotspotControllerConcurrencyApiTest;
  friend class HotspotEnabledStateNotifierTest;

  // Represents hotspot enable or disable control request parameters. Requests
  // are executed as they come in.
  struct HotspotControlRequest {
    HotspotControlRequest(
        bool enabled,
        std::optional<hotspot_config::mojom::DisableReason> disable_reason,
        HotspotControlCallback callback);
    HotspotControlRequest(const HotspotControlRequest&) = delete;
    HotspotControlRequest& operator=(const HotspotControlRequest&) = delete;
    ~HotspotControlRequest();

    bool enabled;
    bool abort = false;
    // Set for disable requests and will be nullopt for enable requests.
    std::optional<hotspot_config::mojom::DisableReason> disable_reason;
    // Tracks the latency of enable hotspot operation and will be nullopt for
    // disable requests.
    std::optional<base::ElapsedTimer> enable_latency_timer;
    HotspotControlCallback callback;
  };

  // TechnologyStateController::HotspotOperationDelegate:
  void PrepareEnableWifi(
      base::OnceCallback<void(bool prepare_success)> callback) override;

  // HotspotStateHandler::Observer:
  void OnHotspotStatusChanged() override;

  void OnCheckTetheringReadiness(
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult result);
  void PerformSetTetheringEnabled(bool enabled);
  void OnSetTetheringEnabledSuccess(const bool& enabled,
                                    const std::string& result);
  void OnSetTetheringEnabledFailure(const bool& enabled,
                                    const std::string& error_name,
                                    const std::string& error_message);
  void OnPrepareEnableHotspotCompleted(bool prepare_success,
                                       bool wifi_turned_off);
  void OnPrepareEnableWifiCompleted(
      base::OnceCallback<void(bool success)> callback,
      hotspot_config::mojom::HotspotControlResult control_result);
  void CompleteCurrentRequest(
      const bool& enabled,
      hotspot_config::mojom::HotspotControlResult result);
  void CompleteEnableRequest(
      hotspot_config::mojom::HotspotControlResult result);
  void CompleteDisableRequest(
      hotspot_config::mojom::HotspotControlResult result);
  void NotifyHotspotTurnedOn();
  void NotifyHotspotTurnedOff(
      hotspot_config::mojom::DisableReason disable_reason);

  std::unique_ptr<HotspotControlRequest> current_enable_request_;
  std::unique_ptr<HotspotControlRequest> current_disable_request_;

  bool allow_hotspot_ = true;
  // Store whether the WiFi was turned off due to the start of hotspot. Need to
  // restore the WiFi status once hotspot is turned off.
  bool wifi_turned_off_ = false;

  raw_ptr<HotspotCapabilitiesProvider> hotspot_capabilities_provider_ = nullptr;
  raw_ptr<HotspotFeatureUsageMetrics> hotspot_feature_usage_metrics_ = nullptr;
  raw_ptr<HotspotStateHandler> hotspot_state_handler_ = nullptr;
  raw_ptr<TechnologyStateController> technology_state_controller_ = nullptr;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<HotspotController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_
