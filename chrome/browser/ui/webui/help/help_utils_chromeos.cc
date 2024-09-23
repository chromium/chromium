// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"

#include <stddef.h>

#include <algorithm>

#include "ash/constants/ash_switches.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace help_utils_chromeos {

bool IsUpdateOverCellularAllowed(bool interactive) {
  // If this is a Cellular First device or the user actively checks for update,
  // the default is to allow updates over cellular.
  bool default_update_over_cellular_allowed =
      interactive ? true : ash::switches::IsCellularFirstDevice();

  // Device Policy overrides the defaults.
  ash::CrosSettings* settings = ash::CrosSettings::Get();
  if (!settings)
    return default_update_over_cellular_allowed;

  const base::Value* types_value =
      settings->GetPref(ash::kAllowedConnectionTypesForUpdate);
  if (!types_value)
    return default_update_over_cellular_allowed;
  CHECK(types_value->is_list());
  const auto& list = types_value->GetList();
  for (size_t i = 0; i < list.size(); ++i) {
    if (!list[i].is_int()) {
      LOG(WARNING) << "Can't parse connection type #" << i;
      continue;
    }

    if (list[i].GetInt() == 4)
      return true;
  }
  // Device policy does not allow updates over cellular, as cellular is not
  // included in allowed connection types for updates.
  return false;
}

}  // namespace help_utils_chromeos
