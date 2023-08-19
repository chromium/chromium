// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace language::prefs {

// The value to use for Accept-Languages HTTP header when making an HTTP
// request. This should not be set directly as it is a combination of
// kSelectedLanguages and kForcedLanguages. To update the list of preferred
// languages, set kSelectedLanguages and this pref will update automatically.
inline constexpr char kAcceptLanguages[] = "intl.accept_languages";

// List which contains the user-selected languages.
inline constexpr char kSelectedLanguages[] = "intl.selected_languages";

// List which contains the policy-forced languages.
inline constexpr char kForcedLanguages[] = "intl.forced_languages";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A string pref (comma-separated list) set to the preferred language IDs
// (ex. "en-US,fr,ko").
inline constexpr char kPreferredLanguages[] =
    "settings.language.preferred_languages";
inline constexpr char kPreferredLanguagesSyncable[] =
    "settings.language.preferred_languages_syncable";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// The application locale as selected by the user, such as "en-AU". This may not
// necessarily be a string locale (a locale that we have strings for on this
// platform). Use |l10n_util::CheckAndResolveLocale| to convert it to a string
// locale if needed, such as "en-GB".
inline constexpr char kApplicationLocale[] = "intl.app_locale";

#if BUILDFLAG(IS_ANDROID)
inline constexpr char kAppLanguagePromptShown[] =
    "language.app_language_prompt_shown";

inline constexpr char kULPLanguages[] = "language.ulp_languages";
#endif

}  // namespace language::prefs

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
