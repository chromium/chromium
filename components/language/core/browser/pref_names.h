// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_

namespace language {
namespace prefs {

extern const char kAcceptLanguages[];

#if defined(OS_CHROMEOS)
extern const char kPreferredLanguages[];
extern const char kPreferredLanguagesSyncable[];
#endif

extern const char kUserLanguageProfile[];

// The application locale.
// DO NOT USE this locale directly: use language::ConvertToActualUILocale()
// after reading it to get the system locale. This pref stores the locale that
// the user selected, if applicable.
extern const char kApplicationLocale[];

extern const char kFluentLanguages[];

}  // namespace prefs
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_PREF_NAMES_H_
