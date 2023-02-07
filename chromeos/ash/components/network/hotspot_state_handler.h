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
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This class caches hotspot related status and implements methods to get
// current state, active client count, capabilities and configure the hotspot
// configurations.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotStateHandler
    : public ShillPropertyChangedObserver,
      public LoginState::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when hotspot state, active client count or hotspot config is
    // changed.
    virtual void OnHotspotStatusChanged() = 0;
  };

  HotspotStateHandler();
  HotspotStateHandler(const HotspotStateHandler&) = delete;
  HotspotStateHandler& operator=(const HotspotStateHandler&) = delete;
  ~HotspotStateHandler() override;

  void Init();

  // Return the latest hotspot state
  const hotspot_config::mojom::HotspotState& GetHotspotState() const;

  // Return the latest hotspot active client count
  size_t GetHotspotActiveClientCount() const;

  // Return the current hotspot configuration
  hotspot_config::mojom::HotspotConfigPtr GetHotspotConfig() const;
  // Return callback for the SetHotspotConfig method. |success| indicates
  // whether the operation is success or not.

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

  // Callback to handle the manager properties with hotspot related properties.
  void OnManagerProperties(absl::optional<base::Value::Dict> properties);

  // Update the cached hotspot_state_ and active_client_count_ from hotspot
  // status in Shill.
  void UpdateHotspotStatus(const base::Value::Dict& status);

  // Notify observers that hotspot state or active client count was changed.
  void NotifyHotspotStatusChanged();

  // Update the cached hotspot_config_ with the tethering configuration
  // from |manager_properties|, and then run the |callback|.
  void UpdateHotspotConfigAndRunCallback(
      SetHotspotConfigCallback callback,
      absl::optional<base::Value::Dict> manager_properties);

  // Callback when the SetHotspotConfig operation succeeded.
  void OnSetHotspotConfigSuccess(SetHotspotConfigCallback callback);

  // Callback when the SetHotspotConfig operation failed.
  void OnSetHotspotConfigFailure(SetHotspotConfigCallback callback,
                                 const std::string& error_name,
                                 const std::string& error_message);

  hotspot_config::mojom::HotspotState hotspot_state_ =
      hotspot_config::mojom::HotspotState::kDisabled;
  absl::optional<base::Value::Dict> hotspot_config_ = absl::nullopt;
  size_t active_client_count_ = 0;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<HotspotStateHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_
