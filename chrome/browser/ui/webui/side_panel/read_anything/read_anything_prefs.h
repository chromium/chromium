// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PREFS_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PREFS_H_

#include "build/build_config.h"
#include "chrome/common/buildflags.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace prefs {

#if !BUILDFLAG(IS_ANDROID)

extern const char kAccessibilityReadAnythingFontName[];
extern const char kAccessibilityReadAnythingFontScale[];
extern const char kAccessibilityReadAnythingColorInfo[];
extern const char kAccessibilityReadAnythingLineSpacing[];
extern const char kAccessibilityReadAnythingLetterSpacing[];

}  // namespace prefs

void RegisterReadAnythingProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PREFS_H_
