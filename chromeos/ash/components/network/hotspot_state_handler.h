// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

// This class caches hotspot related status and implements methods to get
// current state, active client count, capabilities and configure the hotspot
// configurations.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotStateHandler
    : public ShillPropertyChangedObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when hotspot state or active client count is changed.
    virtual void OnHotspotStatusChanged() = 0;
    // Invoked when hotspot state is failed.
    virtual void OnHotspotStateFailed(const std::string& error) = 0;
  };

  HotspotStateHandler();
  HotspotStateHandler(const HotspotStateHandler&) = delete;
  HotspotStateHandler& operator=(const HotspotStateHandler&) = delete;
  ~HotspotStateHandler() override;

  void Init();
  // Return the latest hotspot state
  const chromeos::hotspot_config::mojom::HotspotState& GetHotspotState() const;
  // Return the latest hotspot active client count
  size_t GetHotspotActiveClientCount() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

  // Callback to handle the manager properties with hotspot related properties.
  void OnManagerProperties(absl::optional<base::Value> properties);

  // Update the cached hotspot_state_ and active_client_count_ from hotspot
  // status in Shill.
  void UpdateHotspotStatus(const base::Value& status);

  // Fall back the hotspot state to idle or active when enable/disable hotspot
  // operation is failed.
  void FallbackStateOnFailure();

  // Notify observers that hotspot state or active client count was changed.
  void NotifyHotspotStatusChanged();

  // Notify observers that hotspot state was failure.
  void NotifyHotspotStateFailed(const std::string& error);

  chromeos::hotspot_config::mojom::HotspotState hotspot_state_ =
      chromeos::hotspot_config::mojom::HotspotState::kDisabled;
  size_t active_client_count_ = 0;
  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<HotspotStateHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_STATE_HANDLER_H_
