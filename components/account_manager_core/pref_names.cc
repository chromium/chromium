// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/pref_names.h"

namespace account_manager::prefs {

// A boolean pref to store the list of accounts and their availability in ARC in
// the format: {string gaia_id, bool is_available_in_arc}.
const char kAccountAppsAvailability[] =
    "account_manager.account_apps_availability";

// Keys for `kAccountAppsAvailability`.
const char kIsAvailableInArcKey[] = "is_available_in_arc";

}  // namespace account_manager::prefs
