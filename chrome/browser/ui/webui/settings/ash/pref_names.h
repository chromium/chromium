// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PREF_NAMES_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PREF_NAMES_H_

namespace ash::settings::prefs {

extern const char kSyncOsWallpaper[];

}  // namespace ash::settings::prefs

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::settings::prefs {
using ::ash::settings::prefs::kSyncOsWallpaper;
}

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PREF_NAMES_H_
