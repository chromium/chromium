// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PREFS_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace organizer {

// Registers Organizer Panel specific prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace organizer

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PREFS_H_
