// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_TECHNOLOGY_STATE_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_TECHNOLOGY_STATE_CONTROLLER_H_

#include "base/component_export.h"

#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_type_pattern.h"

namespace ash {

class NetworkStateHandler;

// This class serves as the primary entry point for enabling and disabling
// network devices and technologies, and ensures handling of hotspot/Wifi
// concurrency issues by disabling one before enabling the other.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) TechnologyStateController {
 public:
  TechnologyStateController();
  TechnologyStateController(const TechnologyStateController&) = delete;
  TechnologyStateController& operator=(const TechnologyStateController&) =
      delete;
  ~TechnologyStateController();

  void Init(NetworkStateHandler* network_state_handler);

  // Asynchronously sets the technologies enabled property for |type|.Calls
  // |error_callback| upon failure.
  void SetTechnologiesEnabled(const NetworkTypePattern& type,
                              bool enabled,
                              network_handler::ErrorCallback error_callback);

 private:
  NetworkStateHandler* network_state_handler_ = nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_TECHNOLOGY_STATE_CONTROLLER_H_
