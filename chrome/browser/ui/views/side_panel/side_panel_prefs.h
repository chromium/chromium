// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_PREFS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace side_panel_prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace side_panel_prefs

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_PREFS_H_
