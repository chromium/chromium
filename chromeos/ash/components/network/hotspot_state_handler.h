// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class NetworkStateHandler;

// This class caches hotspot related status and implements methods to get
// current state, active client count, capabilities and configure the hotspot
// configurations.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotStateHandler
    : public ShillPropertyChangedObserver,
      public LoginState::Observer,
      public NetworkStateHandlerObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when hotspot state, active client count or hotspot config is
    // changed.
    virtual void OnHotspotStatusChanged() = 0;
    // Invoked when hotspot capabilities is changed.
    virtual void OnHotspotCapabilitiesChanged() = 0;
  };

  // Represents the hotspot capabilities. Includes:
  // 1. The allow status that is calculated from the combination Shill Tethering
  // Capabilities and Shill Tethering Readiness check result and policy allow
  // status.
  // 2. List of allowed WiFi security modes for WiFi downstream.
  struct HotspotCapabilities {
    HotspotCapabilities(
        const hotspot_config::mojom::HotspotAllowStatus allow_status);
    ~HotspotCapabilities();

    hotspot_config::mojom::HotspotAllowStatus allow_status;
    std::vector<hotspot_config::mojom::WiFiSecurityMode> allowed_security_modes;
  };

  // Represents the check tethering readiness result.
  enum class CheckTetheringReadinessResult {
    kReady = 0,
    kNotAllowed = 1,
    kShillOperationFailed = 2,
  };

  HotspotStateHandler();
  HotspotStateHandler(const HotspotStateHandler&) = delete;
  HotspotStateHandler& operator=(const HotspotStateHandler&) = delete;
  ~HotspotStateHandler() override;

  void Init(NetworkStateHandler* network_state_handler);
  // Return the latest hotspot state
  const hotspot_config::mojom::HotspotState& GetHotspotState() const;
  // Return the latest hotspot active client count
  size_t GetHotspotActiveClientCount() const;
  // Return the current hotspot configuration
  hotspot_config::mojom::HotspotConfigPtr GetHotspotConfig() const;
  // Return the latest hotspot capabilities
  const HotspotCapabilities& GetHotspotCapabilities() const;
  // Return callback for the SetHotspotConfig method. |success| indicates
  // whether the operation is success or not.
  using SetHotspotConfigCallback = base::OnceCallback<void(
      hotspot_config::mojom::SetHotspotConfigResult result)>;

  // Set hotspot configuration with given |config|. |callback| is called with
  // the success result of SetHotspotConfig operation.
  void SetHotspotConfig(hotspot_config::mojom::HotspotConfigPtr config,
                        SetHotspotConfigCallback callback);

  using CheckTetheringReadinessCallback =
      base::OnceCallback<void(CheckTetheringReadinessResult result)>;
  // Check tethering readiness and update the hotspot_capabilities_ if
  // necessary. |callback| is called with check readiness result.
  void CheckTetheringReadiness(CheckTetheringReadinessCallback callback);
  // Set whether Hotspot should be allowed/disallowed by policy.
  void SetPolicyAllowHotspot(bool allow);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

  // LoginState::Observer
  void LoggedInStateChanged() override;

  // NetworkStateHandlerObserver
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // Callback to handle the manager properties with hotspot related properties.
  void OnManagerProperties(absl::optional<base::Value> properties);

  // Update the cached hotspot_state_ and active_client_count_ from hotspot
  // status in Shill.
  void UpdateHotspotStatus(const base::Value& status);

  // Notify observers that hotspot state or active client count was changed.
  void NotifyHotspotStatusChanged();

  // Notify observer that hotspot capabilities was changed.
  void NotifyHotspotCapabilitiesChanged();

  // Update the cached hotspot_config_ with the tethering configuration
  // from |manager_properties|, and then run the |callback|.
  void UpdateHotspotConfigAndRunCallback(
      SetHotspotConfigCallback callback,
      absl::optional<base::Value> manager_properties);

  // Callback when the SetHotspotConfig operation succeeded.
  void OnSetHotspotConfigSuccess(SetHotspotConfigCallback callback);

  // Callback when the SetHotspotConfig operation failed.
  void OnSetHotspotConfigFailure(SetHotspotConfigCallback callback,
                                 const std::string& error_name,
                                 const std::string& error_message);

  // Update the cached hotspot_capabilities_ from the tethering capabilities
  // values from Shill. This function is called whenever the tethering
  // capabilities value is changed in Shill.
  void UpdateHotspotCapabilities(const base::Value& capabilities);

  // Callback when the CheckTetheringReadiness operation succeeded.
  void OnCheckReadinessSuccess(CheckTetheringReadinessCallback callback,
                               const std::string& result);

  // Callback when the CheckTetheringReadiness operation failed.
  void OnCheckReadinessFailure(CheckTetheringReadinessCallback callback,
                               const std::string& error_name,
                               const std::string& error_message);

  // Update the hotspot_capabilities_ with the given |new_allow_status|
  // and then notify observers if it changes.
  void SetHotspotCapablities(
      hotspot_config::mojom::HotspotAllowStatus new_allow_status);

  void ResetNetworkStateHandler();

  hotspot_config::mojom::HotspotState hotspot_state_ =
      hotspot_config::mojom::HotspotState::kDisabled;
  HotspotCapabilities hotspot_capabilities_{
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoCellularUpstream};
  absl::optional<base::Value> hotspot_config_ = absl::nullopt;
  size_t active_client_count_ = 0;

  NetworkStateHandler* network_state_handler_ = nullptr;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<HotspotStateHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_
