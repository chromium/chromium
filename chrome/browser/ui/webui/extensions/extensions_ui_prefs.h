// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_PREFS_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions_ui_prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace extensions_ui_prefs

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSIONS_UI_PREFS_H_
