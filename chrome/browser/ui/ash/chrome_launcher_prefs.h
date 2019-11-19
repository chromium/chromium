// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_
#define CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_

#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

class LauncherControllerHelper;
class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Key for the dictionary entries in the prefs::kPinnedLauncherApps list
// specifying the extension ID of the app to be pinned by that entry.
extern const char kPinnedAppsPrefAppIDKey[];

extern const char kPinnedAppsPrefPinnedByPolicy[];

// To enable finch experiment with number of default apps on the shelf.
// See |kEnableExtendedShelfLayoutParam| in .cc file.
extern const base::Feature kEnableExtendedShelfLayout;

// Value used as a placeholder in the list of pinned applications.
// This is NOT a valid extension identifier so pre-M31 versions ignore it.
extern const char kPinnedAppsPlaceholder[];

void RegisterChromeLauncherUserPrefs(
    user_prefs::PrefRegistrySyncable* registry);

// Init a local pref from a synced pref, if the local pref has no user setting.
// This is used to init shelf alignment and auto-hide on the first user sync.
// The goal is to apply the last elected shelf alignment and auto-hide values
// when a user signs in to a new device for the first time. Otherwise, shelf
// properties are persisted per-display/device. The local prefs are initialized
// with synced (or default) values when when syncing begins, to avoid syncing
// shelf prefs across devices after the very start of the user's first session.
void InitLocalPref(PrefService* prefs, const char* local, const char* synced);

// Gets the ordered list of pinned apps that exist on device from the app sync
// service.
std::vector<ash::ShelfID> GetPinnedAppsFromSync(
    LauncherControllerHelper* helper);

// Removes information about pin position from sync model for the app.
// Note, |shelf_id| with non-empty launch_id is not supported.
void RemovePinPosition(Profile* profile, const ash::ShelfID& shelf_id);

// Updates information about pin position in sync model for the app |shelf_id|.
// |shelf_id_before| optionally specifies an app that exists right before the
// target app. |shelf_ids_after| optionally specifies sorted by position apps
// that exist right after the target app.
// Note, |shelf_id| with non-empty launch_id is not supported.
void SetPinPosition(Profile* profile,
                    const ash::ShelfID& shelf_id,
                    const ash::ShelfID& shelf_id_before,
                    const std::vector<ash::ShelfID>& shelf_ids_after);

#endif  // CHROME_BROWSER_UI_ASH_CHROME_LAUNCHER_PREFS_H_
