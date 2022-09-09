// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_VIEW_PREFS_H_
#define CHROME_BROWSER_UI_BROWSER_VIEW_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

// Register profile-specific preferences specific to BrowserView. These
// preferences may be synced, depending on the pref's |sync_status| parameter.
void RegisterBrowserViewProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

#endif  // CHROME_BROWSER_UI_BROWSER_VIEW_PREFS_H_
