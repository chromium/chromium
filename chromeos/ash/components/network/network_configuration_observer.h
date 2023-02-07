// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIGURATION_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIGURATION_OBSERVER_H_

#include <string>

#include "base/component_export.h"
#include "base/values.h"

namespace ash {

// Observer class for network configuration events (remove only).
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConfigurationObserver {
 public:
  NetworkConfigurationObserver& operator=(const NetworkConfigurationObserver&) =
      delete;

  // Called after a new network configuration is created successfully.
  virtual void OnConfigurationCreated(const std::string& service_path,
                                      const std::string& guid);

  // Called whenever properties on a network configuration are modified.
  virtual void OnConfigurationModified(const std::string& service_path,
                                       const std::string& guid,
                                       const base::Value::Dict* set_properties);

  // Called before a delete is attempted.
  virtual void OnBeforeConfigurationRemoved(const std::string& service_path,
                                            const std::string& guid);

  // Called whenever a network configuration is removed. |service_path|
  // provides the Shill current identifier for the network. |guid| will be set
  // to the corresponding GUID for the network if known at the time of removal,
  // otherwise it will be empty.
  virtual void OnConfigurationRemoved(const std::string& service_path,
                                      const std::string& guid);

  // Called just before the object is destroyed so that observers
  // can safely stop observing.
  virtual void OnShuttingDown();

 protected:
  virtual ~NetworkConfigurationObserver();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIGURATION_OBSERVER_H_
