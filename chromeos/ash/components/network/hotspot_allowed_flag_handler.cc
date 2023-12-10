// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/hotspot_allowed_flag_handler.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

HotspotAllowedFlagHandler::HotspotAllowedFlagHandler() = default;

HotspotAllowedFlagHandler::~HotspotAllowedFlagHandler() {
  if (ShillManagerClient::Get()) {
    ShillManagerClient::Get()->RemovePropertyChangedObserver(this);
  }
}

void HotspotAllowedFlagHandler::Init() {
  ShillManagerClient::Get()->AddPropertyChangedObserver(this);
  ShillManagerClient::Get()->SetProperty(
      shill::kTetheringAllowedProperty,
      base::Value(ash::features::IsHotspotEnabled()), base::DoNothing(),
      base::BindOnce(&HotspotAllowedFlagHandler::OnSetManagerPropertyFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     shill::kTetheringAllowedProperty));
  ShillManagerClient::Get()->SetProperty(
      shill::kExperimentalTetheringFunctionality,
      base::Value(ash::features::IsTetheringExperimentalFunctionalityEnabled()),
      base::DoNothing(),
      base::BindOnce(&HotspotAllowedFlagHandler::OnSetManagerPropertyFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     shill::kExperimentalTetheringFunctionality));
}

void HotspotAllowedFlagHandler::OnSetManagerPropertyFailure(
    const std::string& property_name,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Error setting Shill manager properties: " << property_name
                 << ", error: " << error_name << ", message: " << error_message;
}

void HotspotAllowedFlagHandler::OnPropertyChanged(const std::string& key,
                                                  const base::Value& value) {
  if (key != shill::kTetheringAllowedProperty ||
      value.GetBool() == ash::features::IsHotspotEnabled()) {
    return;
  }

  ShillManagerClient::Get()->SetProperty(
      shill::kTetheringAllowedProperty,
      base::Value(ash::features::IsHotspotEnabled()), base::DoNothing(),
      base::BindOnce(&HotspotAllowedFlagHandler::OnSetManagerPropertyFailure,
                     weak_ptr_factory_.GetWeakPtr(),
                     shill::kTetheringAllowedProperty));
}

}  //  namespace ash