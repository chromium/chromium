// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names_util.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Adds !important to all captions styles. They should always override any
// styles added by the video author or by a user stylesheet. This is because in
// Chrome, there is an option to turn off captions styles, so any time the
// captions are on, the styles should take priority.
std::string AddCSSImportant(std::string css_string) {
  return css_string + " !important";
}

}  // namespace

namespace pref_names_util {

const char kWebKitFontPrefPrefix[] = "webkit.webprefs.fonts.";

bool ParseFontNamePrefPath(const std::string& pref_path,
                           std::string* generic_family,
                           std::string* script) {
  if (!base::StartsWith(pref_path, kWebKitFontPrefPrefix,
                        base::CompareCase::SENSITIVE))
    return false;

  size_t start = strlen(kWebKitFontPrefPrefix);
  size_t pos = pref_path.find('.', start);
  if (pos == std::string::npos || pos + 1 == pref_path.length())
    return false;
  if (generic_family)
    *generic_family = pref_path.substr(start, pos - start);
  if (script)
    *script = pref_path.substr(pos + 1);
  return true;
}

base::Optional<ui::CaptionStyle> GetCaptionStyleFromPrefs(PrefService* prefs) {
  if (!prefs) {
    return base::nullopt;
  }

  ui::CaptionStyle style;

  style.text_size =
      AddCSSImportant(prefs->GetString(prefs::kAccessibilityCaptionsTextSize));
  style.font_family =
      AddCSSImportant(prefs->GetString(prefs::kAccessibilityCaptionsTextFont));
  if (!prefs->GetString(prefs::kAccessibilityCaptionsTextColor).empty()) {
    std::string text_color = base::StringPrintf(
        "rgba(%s,%s)",
        prefs->GetString(prefs::kAccessibilityCaptionsTextColor).c_str(),
        base::NumberToString(
            prefs->GetInteger(prefs::kAccessibilityCaptionsTextOpacity) / 100.0)
            .c_str());
    style.text_color = AddCSSImportant(text_color);
  }

  if (!prefs->GetString(prefs::kAccessibilityCaptionsBackgroundColor).empty()) {
    std::string background_color = base::StringPrintf(
        "rgba(%s,%s)",
        prefs->GetString(prefs::kAccessibilityCaptionsBackgroundColor).c_str(),
        base::NumberToString(
            prefs->GetInteger(prefs::kAccessibilityCaptionsBackgroundOpacity) /
            100.0)
            .c_str());
    style.background_color = AddCSSImportant(background_color);
  }

  style.text_shadow = AddCSSImportant(
      prefs->GetString(prefs::kAccessibilityCaptionsTextShadow));

  return style;
}

}  // namespace pref_names_util
