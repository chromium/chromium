// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_PREFS_H_
#define CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chrome_labs_prefs {

extern const char kBrowserLabsEnabled[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace chrome_labs_prefs

#endif  // CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_PREFS_H_
