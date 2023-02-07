// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_METADATA_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_METADATA_OBSERVER_H_

#include <string>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "base/values.h"

namespace ash {

// Observer class for events that affect network metadata.  All callbacks are
// executed after related metadata has been updated.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkMetadataObserver
    : public base::CheckedObserver {
 public:
  // Called the first time that a network is successfully connected to.
  virtual void OnFirstConnectionToNetwork(const std::string& guid);

  // Called after a network configuration and associated metadata has been
  // created.
  virtual void OnNetworkCreated(const std::string& guid);

  // Called after a network configuration and associated metadata has been
  // updated.
  virtual void OnNetworkUpdate(const std::string& guid,
                               const base::Value::Dict* set_properties);

 protected:
  NetworkMetadataObserver();
  ~NetworkMetadataObserver() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_METADATA_OBSERVER_H_
