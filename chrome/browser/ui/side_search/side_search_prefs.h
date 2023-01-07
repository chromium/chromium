// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_PREFS_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace side_search_prefs {

extern const char kSideSearchEnabled[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace side_search_prefs

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_PREFS_H_
