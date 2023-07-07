// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/pref_names.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace language {
namespace prefs {

// The value to use for Accept-Languages HTTP header when making an HTTP
// request. This should not be set directly as it is a combination of
// kSelectedLanguages and kForcedLanguages. To update the list of preferred
// languages, set kSelectedLanguages and this pref will update automatically.
const char kAcceptLanguages[] = "intl.accept_languages";

// List which contains the user-selected languages.
const char kSelectedLanguages[] = "intl.selected_languages";

// List which contains the policy-forced languages.
const char kForcedLanguages[] = "intl.forced_languages";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A string pref (comma-separated list) set to the preferred language IDs
// (ex. "en-US,fr,ko").
const char kPreferredLanguages[] = "settings.language.preferred_languages";
const char kPreferredLanguagesSyncable[] =
    "settings.language.preferred_languages_syncable";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Important: Refer to header file for how to use this.
const char kApplicationLocale[] = "intl.app_locale";

#if BUILDFLAG(IS_ANDROID)
const char kAppLanguagePromptShown[] = "language.app_language_prompt_shown";

const char kULPLanguages[] = "language.ulp_languages";
#endif

}  // namespace prefs
}  // namespace language
