// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_prefs.h"

#include "base/values.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/common/read_anything/read_anything_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ui/accessibility/accessibility_features.h"

#if !BUILDFLAG(IS_ANDROID)

void RegisterReadAnythingProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kAccessibilityReadAnythingFontName,
                               // All languages use the same default font.
                               GetSupportedFonts("en").front(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kAccessibilityReadAnythingFontScale, 2.0f,
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
  // TODO(crbug.com/40927698): When we release on multiple platforms, add
  // separate prefs for voices on each platform since they're not always
  // the same on every platform.
  registry->RegisterDictionaryPref(
      prefs::kAccessibilityReadAnythingVoiceName, base::DictValue(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kAccessibilityReadAnythingSpeechRate, 1.0,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityReadAnythingHighlightGranularity,
      static_cast<int>(
          read_anything::mojom::HighlightGranularity::kDefaultValue),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kAccessibilityReadAnythingLanguagesEnabled,
                             base::ListValue(),
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityReadAnythingLinksEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  if (features::IsImmersiveReadAnythingEnabled()) {
    registry->RegisterBooleanPref(
        prefs::kAccessibilityReadAnythingImagesEnabled, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  } else {
    registry->RegisterBooleanPref(
        prefs::kAccessibilityReadAnythingImagesEnabled, false,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  }
  if (features::IsReadAnythingOmniboxChipEnabled() &&
      base::FeatureList::IsEnabled(features::kPageActionsMigration)) {
    registry->RegisterIntegerPref(
        prefs::kAccessibilityReadAnythingOmniboxChipIgnoredCount, 0,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  }
  if (features::IsReadAnythingLineFocusEnabled()) {
    registry->RegisterIntegerPref(
        prefs::kAccessibilityReadAnythingLineFocus,
        static_cast<int>(read_anything::mojom::LineFocus::kDefaultValue),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    registry->RegisterIntegerPref(
        prefs::kAccessibilityReadAnythingLastNonDisabledLineFocus,
        static_cast<int>(read_anything::mojom::LineFocus::kDefaultValue),
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)
