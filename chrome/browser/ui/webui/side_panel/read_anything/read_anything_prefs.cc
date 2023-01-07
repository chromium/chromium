// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace prefs {

#if !BUILDFLAG(IS_ANDROID)
// String to represent the user's preferred font name for the read anything UI.
const char kAccessibilityReadAnythingFontName[] =
    "settings.a11y.read_anything.font_name";

// Double to represent the user's preferred font size scaling factor.
const char kAccessibilityReadAnythingFontScale[] =
    "settings.a11y.read_anything.font_scale";

// Int value to represent the user's preferred color settings.
const char kAccessibilityReadAnythingColorInfo[] =
    "settings.a11y.read_anything.color_info";

// Int value to represent the user's preferred line spacing setting.
const char kAccessibilityReadAnythingLineSpacing[] =
    "settings.a11y.read_anything.line_spacing";

// Int value to represent the user's preferred letter spacing setting.
const char kAccessibilityReadAnythingLetterSpacing[] =
    "settings.a11y.read_anything.letter_spacing";

}  // namespace prefs

void RegisterReadAnythingProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kAccessibilityReadAnythingFontName,
                               kReadAnythingDefaultFontName,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kAccessibilityReadAnythingFontScale,
                               kReadAnythingDefaultFontScale,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityReadAnythingColorInfo,
      (int)read_anything::mojom::Colors::kDefaultValue,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityReadAnythingLineSpacing,
      (int)read_anything::mojom::Spacing::kDefault,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityReadAnythingLetterSpacing,
      (int)read_anything::mojom::Spacing::kDefault,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

#endif  // !BUILDFLAG(IS_ANDROID)
