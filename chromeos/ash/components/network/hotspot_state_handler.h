// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"

namespace ash {

// This class caches hotspot related status and implements methods to get
// current state and active client count.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotStateHandler
    : public ShillPropertyChangedObserver {
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

  // Returns the reason for hotspot being disabled. nullopt is returned when the
  // disable reason isn't set
  const std::optional<hotspot_config::mojom::DisableReason> GetDisableReason()
      const;

  // Return the latest hotspot active client count
  size_t GetHotspotActiveClientCount() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  // Stores the count of the active client and enables the system wake lock when
  // the count is not 0.
  class ActiveClientCount {
   public:
    ActiveClientCount();
    ~ActiveClientCount();

    // Setter/getter method for the active client count.
    void Set(size_t value);
    size_t Get() const;

   private:
    // Enables/Disables the system wake lock.
    void EnableWakeLock();
    void DisableWakeLock();

    // The value of the active client count.
    size_t value_ = 0;

    // The wake lock id. It has values if and only if the wake lock is enabled.
    std::optional<int> wake_lock_id_ = std::nullopt;
  };

  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

  // Callback to handle the manager properties with hotspot related properties.
  void OnManagerProperties(std::optional<base::Value::Dict> properties);

  // Update the cached hotspot_state_ and active_client_count_ from hotspot
  // status in Shill.
  void UpdateHotspotStatus(const base::Value::Dict& status);

  // Updates the reason for hotspot getting disabled and notifies observers.
  void UpdateDisableReason(const base::Value::Dict& status);

  // Notify observers that hotspot state or active client count was changed.
  void NotifyHotspotStatusChanged();

  hotspot_config::mojom::HotspotState hotspot_state_ =
      hotspot_config::mojom::HotspotState::kDisabled;

  std::optional<hotspot_config::mojom::DisableReason> disable_reason_ =
      std::nullopt;

  ActiveClientCount active_client_count_;

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<HotspotStateHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_
