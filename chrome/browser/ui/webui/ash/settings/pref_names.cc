// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pref_names.h"

namespace ash::settings::prefs {

// Boolean specifying whether OS wallpaper sync is enabled. This is stored
// separately from the other OS sync preferences because it's an edge case;
// wallpaper does not have its own ModelType, so it cannot be part of
// UserSelectableOsType like the other OS sync types.
//
// Please note that this is only relevant if IsSyncAllOsTypesEnabled() is false,
// so callers have to check both this pref and IsSyncAllOsTypesEnabled() to
// verify whether this wallpaper sync is enabled.
// TODO(https://crbug.com/1318106): Create a helper method that checks both.
const char kSyncOsWallpaper[] = "sync.os_wallpaper";

}  // namespace ash::settings::prefs
