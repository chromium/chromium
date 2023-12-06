// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_SCHEDULER_CONFIGURATION_MANAGER_BASE_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_SCHEDULER_CONFIGURATION_MANAGER_BASE_H_

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/component_export.h"
#include "base/observer_list.h"

namespace ash {

// A base class for SchedulerConfigurationManager.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
    SchedulerConfigurationManagerBase {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when SetSchedulerConfiguration D-Bus call to debugd returns.
    virtual void OnConfigurationSet(bool success,
                                    size_t num_cores_disabled) = 0;
  };

  SchedulerConfigurationManagerBase();

  SchedulerConfigurationManagerBase(const SchedulerConfigurationManagerBase&) =
      delete;
  SchedulerConfigurationManagerBase& operator=(
      const SchedulerConfigurationManagerBase&) = delete;

  virtual ~SchedulerConfigurationManagerBase();

  // Gets the most recent reply from debugd for SetSchedulerConfiguration D-Bus
  // call. Returns nullopt when the D-Bus client hasn't received any replies
  // yet.
  virtual std::optional<std::pair<bool, size_t>> GetLastReply() const = 0;

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 protected:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_SCHEDULER_CONFIGURATION_MANAGER_BASE_H_
