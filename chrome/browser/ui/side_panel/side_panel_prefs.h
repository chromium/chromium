// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_PREFS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_PREFS_H_

#include "base/values.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace side_panel_prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

base::ListValue GetConfigurableSidePanelAlignments(Profile* profile);

}  // namespace side_panel_prefs

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_PREFS_H_
