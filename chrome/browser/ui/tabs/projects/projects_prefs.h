// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PROJECTS_PROJECTS_PREFS_H_
#define CHROME_BROWSER_UI_TABS_PROJECTS_PROJECTS_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace projects {

// Registers Projects Panel specific prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace projects

#endif  // CHROME_BROWSER_UI_TABS_PROJECTS_PROJECTS_PREFS_H_
