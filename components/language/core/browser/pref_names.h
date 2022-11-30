// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace language {
namespace prefs {

extern const char kAcceptLanguages[];

extern const char kSelectedLanguages[];

extern const char kForcedLanguages[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kPreferredLanguages[];
extern const char kPreferredLanguagesSyncable[];
#endif

// The application locale as selected by the user, such as "en-AU". This may not
// necessarily be a string locale (a locale that we have strings for on this
// platform). Use |l10n_util::CheckAndResolveLocale| to convert it to a string
// locale if needed, such as "en-GB".
extern const char kApplicationLocale[];

#if BUILDFLAG(IS_ANDROID)
extern const char kAppLanguagePromptShown[];

extern const char kULPLanguages[];
#endif

}  // namespace prefs
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
