// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/network/hotspot_capabilities_provider.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/services/hotspot_config/public/cpp/hotspot_enabled_state_provider.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-forward.h"

namespace ash {

// Handles enable or disable hotspot.
//
// Enabling the hotspot involves the following operations:
// 1. Check hotspot capabilities
// 2. Check tethering readiness
// 3. Enable tethering from Shill
//
// Enable or disable requests are queued and executes one request at a time in
// order.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotController
    : public TechnologyStateController::HotspotOperationDelegate,
      public hotspot_config::HotspotEnabledStateProvider {
 public:
  HotspotController();
  HotspotController(const HotspotController&) = delete;
  HotspotController& operator=(const HotspotController&) = delete;
  ~HotspotController() override;

  void Init(HotspotCapabilitiesProvider* hotspot_capabilities_provider,
            HotspotStateHandler* hotspot_state_handler,
            TechnologyStateController* technolog_state_controller);

  // Return callback for the EnableHotspot or DisableHotspot method.
  using HotspotControlCallback = base::OnceCallback<void(
      hotspot_config::mojom::HotspotControlResult control_result)>;

  // Push the enable or disable hotspot request to the request queue and try to
  // execute. If another request is already being processed, the current request
  // will wait until the previous one is completed.
  void EnableHotspot(HotspotControlCallback callback);
  void DisableHotspot(HotspotControlCallback callback,
                      hotspot_config::mojom::DisableReason disable_reason);

  // Restart hotspot if hotspot is active.
  void RestartHotspotIfActive();

  // Set whether Hotspot should be allowed/disallowed by policy.
  void SetPolicyAllowHotspot(bool allow_hotspot);

 private:
  friend class HotspotControllerTest;

  // Represents hotspot enable or disable control request parameters. Requests
  // are queued and processed one at a time.
  struct HotspotControlRequest {
    HotspotControlRequest(
        bool enabled,
        absl::optional<hotspot_config::mojom::DisableReason> disable_reason,
        HotspotControlCallback callback);
    HotspotControlRequest(const HotspotControlRequest&) = delete;
    HotspotControlRequest& operator=(const HotspotControlRequest&) = delete;
    ~HotspotControlRequest();

    bool enabled;
    bool wifi_turned_off = false;
    // Set for disable requests and will be nullopt for enable requests.
    absl::optional<hotspot_config::mojom::DisableReason> disable_reason;
    // Tracks the latency of enable hotspot operation and will be nullopt for
    // disable requests.
    absl::optional<base::ElapsedTimer> enable_latency_timer;
    HotspotControlCallback callback;
  };

  // TechnologyStateController::HotspotOperationDelegate:
  void PrepareEnableWifi(
      base::OnceCallback<void(bool prepare_success)> callback) override;

  void ProcessRequestQueue();
  void CheckTetheringReadiness();
  void OnCheckTetheringReadiness(
      HotspotCapabilitiesProvider::CheckTetheringReadinessResult result);
  void PerformSetTetheringEnabled(bool enabled);
  void OnSetTetheringEnabledSuccess(const std::string& result);
  void OnSetTetheringEnabledFailure(const std::string& error_name,
                                    const std::string& error_message);
  void OnPrepareEnableHotspotCompleted(bool prepare_success,
                                       bool wifi_turned_off);
  void OnPrepareEnableWifiCompleted(
      base::OnceCallback<void(bool success)> callback,
      hotspot_config::mojom::HotspotControlResult control_result);
  void OnDisableHotspotCompleteForRestart(
      hotspot_config::mojom::HotspotControlResult control_result);
  bool IsCurrentRequestAlreadyFulfilled();
  void CompleteCurrentRequest(
      hotspot_config::mojom::HotspotControlResult result);

  std::unique_ptr<HotspotControlRequest> current_request_;
  base::queue<std::unique_ptr<HotspotControlRequest>> queued_requests_;
  bool allow_hotspot_ = true;
  HotspotCapabilitiesProvider* hotspot_capabilities_provider_ = nullptr;
  HotspotStateHandler* hotspot_state_handler_ = nullptr;
  TechnologyStateController* technology_state_controller_ = nullptr;

  base::WeakPtrFactory<HotspotController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONTROLLER_H_
