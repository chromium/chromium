// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CAPABILITIES_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CAPABILITIES_PROVIDER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"

namespace ash {

class NetworkStateHandler;

// This class caches hotspot related status and implements methods to get
// current state, active client count, capabilities and configure the hotspot
// configurations.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotCapabilitiesProvider
    : public ShillPropertyChangedObserver,
      public NetworkStateHandlerObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when hotspot capabilities is changed.
    virtual void OnHotspotCapabilitiesChanged() = 0;
  };

  // Represents the hotspot capabilities. Includes:
  // 1. The allow status that is calculated from the combination Shill Tethering
  // Capabilities and Shill Tethering Readiness check result and policy allow
  // status.
  // 2. List of allowed WiFi security modes for WiFi downstream.
  struct HotspotCapabilities {
    explicit HotspotCapabilities(
        const hotspot_config::mojom::HotspotAllowStatus allow_status);
    ~HotspotCapabilities();

    hotspot_config::mojom::HotspotAllowStatus allow_status;
    std::vector<hotspot_config::mojom::WiFiSecurityMode> allowed_security_modes;
  };

  // Represents the check tethering readiness result.
  enum class CheckTetheringReadinessResult {
    kReady = 0,
    kNotAllowed = 1,
    kNotAllowedByCarrier = 2,
    kNotAllowedOnFW = 3,
    kNotAllowedOnVariant = 4,
    kNotAllowedUserNotEntitled = 5,
    kUpstreamNetworkNotAvailable = 6,
    kShillOperationFailed = 7,
    kUnknownResult = 8,
  };

  HotspotCapabilitiesProvider();
  HotspotCapabilitiesProvider(const HotspotCapabilitiesProvider&) = delete;
  HotspotCapabilitiesProvider& operator=(const HotspotCapabilitiesProvider&) =
      delete;
  ~HotspotCapabilitiesProvider() override;

  void Init(NetworkStateHandler* network_state_handler,
            HotspotAllowedFlagHandler* hotspot_allowed_flag_handler);

  // Return the latest hotspot capabilities
  const HotspotCapabilities& GetHotspotCapabilities() const;

  // Return callback for the CheckTetheringReadiness method.
  using CheckTetheringReadinessCallback =
      base::OnceCallback<void(CheckTetheringReadinessResult result)>;

  // Check tethering readiness and update the hotspot_capabilities_ if
  // necessary. |callback| is called with check readiness result.
  void CheckTetheringReadiness(CheckTetheringReadinessCallback callback);

  void SetPolicyAllowed(bool allowed);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  friend class HotspotMetricsHelperTest;
  friend class HotspotFeatureUsageMetricsTest;
  friend class HotspotControllerTest;
  friend class HotspotControllerConcurrencyApiTest;

  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

  // NetworkStateHandlerObserver
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

  // Callback to handle the manager properties with hotspot related properties.
  void OnManagerProperties(std::optional<base::Value::Dict> properties);

  // Update the hotspot allow status with the given |new_allow_status|
  // and then notify observers if it changes.
  void SetHotspotAllowStatus(
      hotspot_config::mojom::HotspotAllowStatus new_allow_status);

  // Notify observer that hotspot capabilities was changed.
  void NotifyHotspotCapabilitiesChanged();

  // Update the cached hotspot_capabilities_ from the tethering capabilities
  // values from Shill. This function is called whenever the tethering
  // capabilities value is changed in Shill.
  void UpdateHotspotCapabilities(const base::Value::Dict& capabilities);

  // Callback when the CheckTetheringReadiness operation succeeded.
  void OnCheckReadinessSuccess(CheckTetheringReadinessCallback callback,
                               const std::string& result);

  // Callback when the CheckTetheringReadiness operation failed.
  void OnCheckReadinessFailure(CheckTetheringReadinessCallback callback,
                               const std::string& error_name,
                               const std::string& error_message);

  void ResetNetworkStateHandler();

  HotspotCapabilities hotspot_capabilities_{
      hotspot_config::mojom::HotspotAllowStatus::kDisallowedNoCellularUpstream};

  bool policy_allow_hotspot_ = true;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  raw_ptr<HotspotAllowedFlagHandler> hotspot_allowed_flag_handler_;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<HotspotCapabilitiesProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CAPABILITIES_PROVIDER_H_
