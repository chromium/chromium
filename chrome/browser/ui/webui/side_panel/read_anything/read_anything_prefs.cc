// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"

#include "base/values.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ui/accessibility/accessibility_features.h"

#if !BUILDFLAG(IS_ANDROID)

void RegisterReadAnythingProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kAccessibilityReadAnythingFontName,
      string_constants::kReadAnythingPlaceholderFontName,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kAccessibilityReadAnythingFontScale,
                               kReadAnythingDefaultFontScale,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityReadAnythingColorInfo,
      static_cast<int>(read_anything::mojom::Colors::kDefaultValue),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityReadAnythingLineSpacing,
      static_cast<int>(read_anything::mojom::LineSpacing::kDefaultValue),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityReadAnythingLetterSpacing,
      static_cast<int>(read_anything::mojom::LetterSpacing::kDefaultValue),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  if (features::IsReadAnythingReadAloudEnabled()) {
    // TODO(crbug.com/40927698): When we release on multiple platforms, add
    // separate prefs for voices on each platform since they're not always
    // the same on every platform.
    if (features::IsReadAloudAutoVoiceSwitchingEnabled()) {
      registry->RegisterDictionaryPref(
          prefs::kAccessibilityReadAnythingVoiceName, base::Value::Dict(),
          user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    } else {
      registry->RegisterStringPref(
          prefs::kAccessibilityReadAnythingVoiceName,
          string_constants::kReadAnythingPlaceholderVoiceName,
          user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    }
    registry->RegisterDoublePref(
        prefs::kAccessibilityReadAnythingSpeechRate,
        kReadAnythingDefaultSpeechRate,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    registry->RegisterIntegerPref(
        prefs::kAccessibilityReadAnythingHighlightGranularity,
        static_cast<int>(
            read_anything::mojom::HighlightGranularity::kDefaultValue),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    // TODO(crbug.com/40927698): Update the default value for this integer
    // pref to be an enum value, like the ones above
    registry->RegisterIntegerPref(
        prefs::kAccessibilityReadAnythingHighlightColor, 0,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    registry->RegisterListPref(
        prefs::kAccessibilityReadAnythingLanguagesEnabled, base::Value::List(),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  }
  registry->RegisterBooleanPref(
      prefs::kAccessibilityReadAnythingLinksEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kAccessibilityReadAnythingImagesEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

#endif  // !BUILDFLAG(IS_ANDROID)
