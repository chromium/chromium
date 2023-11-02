// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_
#define CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

void RegisterBrowserPrefs(PrefRegistrySimple* registry);
void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry);

#endif  // CHROME_BROWSER_UI_BROWSER_UI_PREFS_H_
