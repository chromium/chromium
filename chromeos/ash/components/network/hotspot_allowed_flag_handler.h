// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ALLOWED_FLAG_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ALLOWED_FLAG_HANDLER_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace ash {

// Handles setting shill::kExperimentalTetheringFunctionality based on the
// kTetheringExperimentalFunctionality feature flag. This manager property value
// is updated when the handler initializes and when UpdateFlags() is called.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotAllowedFlagHandler {
 public:
  HotspotAllowedFlagHandler();
  HotspotAllowedFlagHandler(const HotspotAllowedFlagHandler&) = delete;
  HotspotAllowedFlagHandler& operator=(const HotspotAllowedFlagHandler&) =
      delete;
  ~HotspotAllowedFlagHandler();

  void Init();

  // Refreshes the kExperimentalTetheringFunctionality flags in shill based on
  // the current feature flag state.
  void UpdateFlags();

 private:

  // Callback when set shill manager property operation failed.
  void OnSetManagerPropertyFailure(const std::string& property_name,
                                   const std::string& error_name,
                                   const std::string& error_message);

  base::WeakPtrFactory<HotspotAllowedFlagHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ALLOWED_FLAG_HANDLER_H_
