// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PREFS_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PREFS_H_

#include "build/build_config.h"
#include "chrome/common/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace prefs {

// String to represent the user's preferred font name for the read anything UI.
inline constexpr char kAccessibilityReadAnythingFontName[] =
    "settings.a11y.read_anything.font_name";

// Double to represent the user's preferred font size scaling factor.
inline constexpr char kAccessibilityReadAnythingFontScale[] =
    "settings.a11y.read_anything.font_scale";

// Int value to represent the user's preferred color settings.
inline constexpr char kAccessibilityReadAnythingColorInfo[] =
    "settings.a11y.read_anything.color_info";

// Int value to represent the user's preferred line spacing setting.
inline constexpr char kAccessibilityReadAnythingLineSpacing[] =
    "settings.a11y.read_anything.line_spacing";

// Int value to represent the user's preferred letter spacing setting.
inline constexpr char kAccessibilityReadAnythingLetterSpacing[] =
    "settings.a11y.read_anything.letter_spacing";

// String to represent the user's preferred voice for reading aloud.
// With auto voice switching on, it's a map to represent the user's preferred
// voice per language for reading aloud.
// TODO(crbug.com/40927698): Rename to kAccessibilityReadAnythingVoices when we
// enable automatic voice switching.
inline constexpr char kAccessibilityReadAnythingVoiceName[] =
    "settings.a11y.read_anything.voice_name";

// Double to represent the user's preferred speech rate setting.
inline constexpr char kAccessibilityReadAnythingSpeechRate[] =
    "settings.a11y.read_anything.speech_rate";

// Int value to represent the user's preferred granularity for highlighting as
// text is read.
inline constexpr char kAccessibilityReadAnythingHighlightGranularity[] =
    "settings.a11y.read_anything.highlight_granularity";

// Int value to represent the user's preferred color for highlighting as text
// is read.
inline constexpr char kAccessibilityReadAnythingHighlightColor[] =
    "settings.a11y.read_anything.highlight_color";

inline constexpr char kAccessibilityReadAnythingLinksEnabled[] =
    "settings.a11y.read_anything.links_enabled";

inline constexpr char kAccessibilityReadAnythingImagesEnabled[] =
    "settings.a11y.read_anything.images_enabled";

// List of strings to represent the user's preferred
// languages for the read anything UI.
inline constexpr char kAccessibilityReadAnythingLanguagesEnabled[] =
    "settings.a11y.read_anything.languages_enabled";

}  // namespace prefs

void RegisterReadAnythingProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_PREFS_H_
