// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/pref_names.h"

namespace language {
namespace prefs {

// The value to use for Accept-Languages HTTP header when making an HTTP
// request.
const char kAcceptLanguages[] = "intl.accept_languages";

#if defined(OS_CHROMEOS)
// A string pref (comma-separated list) set to the preferred language IDs
// (ex. "en-US,fr,ko").
const char kPreferredLanguages[] = "settings.language.preferred_languages";
const char kPreferredLanguagesSyncable[] =
    "settings.language.preferred_languages_syncable";
#endif  // defined(OS_CHROMEOS)

// The JSON representation of the user's language profile. Used as an input to
// the user language model (i.e. for determining which languages a user
// understands).
const char kUserLanguageProfile[] = "language_profile";

// Important: Refer to header file for how to use this.
const char kApplicationLocale[] = "intl.app_locale";

// Originally translate blocked languages from TranslatePrefs.
const char kFluentLanguages[] = "translate_blocked_languages";

}  // namespace prefs
}  // namespace language
