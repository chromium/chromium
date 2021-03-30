// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"

#include <stddef.h>

#include <algorithm>

#include "ash/constants/ash_switches.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"

namespace help_utils_chromeos {

bool IsUpdateOverCellularAllowed(bool interactive) {
  // If this is a Cellular First device or the user actively checks for update,
  // the default is to allow updates over cellular.
  bool default_update_over_cellular_allowed =
      interactive ? true : chromeos::switches::IsCellularFirstDevice();

  // Device Policy overrides the defaults.
  ash::CrosSettings* settings = ash::CrosSettings::Get();
  if (!settings)
    return default_update_over_cellular_allowed;

  const base::Value* raw_types_value =
      settings->GetPref(chromeos::kAllowedConnectionTypesForUpdate);
  if (!raw_types_value)
    return default_update_over_cellular_allowed;
  const base::ListValue* types_value;
  CHECK(raw_types_value->GetAsList(&types_value));
  for (size_t i = 0; i < types_value->GetSize(); ++i) {
    int connection_type;
    if (!types_value->GetInteger(i, &connection_type)) {
      LOG(WARNING) << "Can't parse connection type #" << i;
      continue;
    }
    if (connection_type == 4)
      return true;
  }
  // Device policy does not allow updates over cellular, as cellular is not
  // included in allowed connection types for updates.
  return false;
}

}  // namespace help_utils_chromeos
