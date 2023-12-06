// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONFIGURATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONFIGURATION_HANDLER_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"

namespace ash {

// This class caches hotspot configurations and implements methods to get and
// update the hotspot configurations.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotConfigurationHandler
    : public ShillPropertyChangedObserver,
      public LoginState::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when hotspot config is changed.
    virtual void OnHotspotConfigurationChanged() = 0;
  };

  HotspotConfigurationHandler();
  HotspotConfigurationHandler(const HotspotConfigurationHandler&) = delete;
  HotspotConfigurationHandler& operator=(const HotspotConfigurationHandler&) =
      delete;
  ~HotspotConfigurationHandler() override;

  void Init();

  // Return the current hotspot configuration
  hotspot_config::mojom::HotspotConfigPtr GetHotspotConfig() const;

  // Return callback for the SetHotspotConfig method. |result| indicates
  // the set configuration operation result.
  using SetHotspotConfigCallback = base::OnceCallback<void(
      hotspot_config::mojom::SetHotspotConfigResult result)>;

  // Set hotspot configuration with given |config|. |callback| is called with
  // the success result of SetHotspotConfig operation.
  void SetHotspotConfig(hotspot_config::mojom::HotspotConfigPtr config,
                        SetHotspotConfigCallback callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

  // LoginState::Observer
  void LoggedInStateChanged() override;

  // Notify observers that hotspot state or active client count was changed.
  void NotifyHotspotConfigurationChanged();

  // Update the cached hotspot_config_ with the tethering configuration
  // from |manager_properties|, and then run the |callback|.
  void UpdateHotspotConfigAndRunCallback(
      SetHotspotConfigCallback callback,
      std::optional<base::Value::Dict> manager_properties);

  // Callback when the SetHotspotConfig operation succeeded.
  void OnSetHotspotConfigSuccess(SetHotspotConfigCallback callback);

  // Callback when the SetHotspotConfig operation failed.
  void OnSetHotspotConfigFailure(SetHotspotConfigCallback callback,
                                 const std::string& error_name,
                                 const std::string& error_message);

  std::optional<base::Value::Dict> hotspot_config_;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<HotspotConfigurationHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_CONFIGURATION_HANDLER_H_
