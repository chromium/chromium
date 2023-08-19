// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_PREF_NAMES_H_
#define COMPONENTS_SPELLCHECK_BROWSER_PREF_NAMES_H_

namespace spellcheck::prefs {

// Boolean pref to define the default values for using spellchecker.
inline constexpr char kSpellCheckEnable[] = "browser.enable_spellchecking";

// String which represents the dictionary name for our spell-checker.
// This is an old preference that is being migrated to kSpellCheckDictionaries.
inline constexpr char kSpellCheckDictionary[] = "spellcheck.dictionary";

// List of strings representing the dictionary names for our spell-checker.
inline constexpr char kSpellCheckDictionaries[] = "spellcheck.dictionaries";

// List of strings representing the dictionary names for languages that are
// force-enabled in our spell-checker due to the SpellcheckLanguage policy.
inline constexpr char kSpellCheckForcedDictionaries[] =
    "spellcheck.forced_dictionaries";

// List of strings representing the dictionary names for languages that are
// force-disabled in our spell-checker due to the SpellcheckLanguageBlocklist
// policy.
inline constexpr char kSpellCheckBlocklistedDictionaries[] =
    "spellcheck.blocked_dictionaries";

// String which represents whether we use the spelling service.
inline constexpr char kSpellCheckUseSpellingService[] =
    "spellcheck.use_spelling_service";

}  // namespace spellcheck::prefs

#endif  // COMPONENTS_SPELLCHECK_BROWSER_PREF_NAMES_H_
