// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PREF_NAMES_UTIL_H_
#define CHROME_COMMON_PREF_NAMES_UTIL_H_

#include <string>

#include "components/prefs/pref_service.h"
#include "ui/native_theme/native_theme.h"

namespace pref_names_util {

// Prefs prefix for all font types. Ends in a period.
extern const char kWebKitFontPrefPrefix[];

// Extracts the generic family and script from font name pref path |pref_path|.
// For example, if |pref_path| is "webkit.webprefs.fonts.serif.Hang", returns
// true and sets |generic_family| to "serif" and |script| to "Hang".
bool ParseFontNamePrefPath(const std::string& pref_path,
                           std::string* generic_family,
                           std::string* script);

// Constructs the CaptionStyle struct from the caption-related preferences.
base::Optional<ui::CaptionStyle> GetCaptionStyleFromPrefs(PrefService* prefs);

}  // namespace pref_names_util

#endif  // CHROME_COMMON_PREF_NAMES_UTIL_H_
