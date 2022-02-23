// Copyright 2017 The Chromium Authors. All rights reserved.
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

// The application locale.
// DO NOT USE this locale directly: use language::ConvertToActualUILocale()
// after reading it to get the system locale. This pref stores the locale that
// the user selected, if applicable.
extern const char kApplicationLocale[];

#if BUILDFLAG(IS_ANDROID)
extern const char kAppLanguagePromptShown[];
#endif

}  // namespace prefs
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
