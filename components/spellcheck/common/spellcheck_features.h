// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_FEATURES_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace spellcheck {

#if BUILDFLAG(ENABLE_SPELLCHECK)

bool UseBrowserSpellChecker();

#if BUILDFLAG(IS_WIN)
// Makes UseBrowserSpellChecker() return false in a scope.
//
// The non-browser spell checker (Hunspell) is used when the user hasn't
// installed the required Language Packs in the Windows settings. Disabling the
// browser spell checker allows testing it.
class ScopedDisableBrowserSpellCheckerForTesting {
 public:
  ScopedDisableBrowserSpellCheckerForTesting();
  ~ScopedDisableBrowserSpellCheckerForTesting();

 private:
  const bool previous_value_;
};

// If the kWinDelaySpellcheckServiceInit feature flag is enabled, don't
// initialize the spellcheck dictionaries when the SpellcheckService is
// instantiated. With this flag set: (1) Completing the initialization of the
// spellcheck service is on-demand, invoked by calling
// SpellcheckService::InitializeDictionaries with a callback to indicate when
// the operation completes. (2) The call to create the spellcheck service in
// ChromeBrowserMainParts::PreMainMessageLoopRunImpl will be skipped. Chromium
// will still by default instantiate the spellcheck service on startup for
// custom dictionary synchronization, but will not load Windows spellcheck
// dictionaries. The command line for launching the browser with Windows hybrid
// spellchecking enabled but no initialization of the spellcheck service is:
//    chrome
//    --enable-features=WinDelaySpellcheckServiceInit
// and if instantiation of the spellcheck service needs to be completely
// disabled:
//     chrome
//    --enable-features=WinDelaySpellcheckServiceInit
//    --disable-sync-types="Dictionary"
BASE_DECLARE_FEATURE(kWinDelaySpellcheckServiceInit);

// When set, do not perform the expensive operation of retrieving suggestions
// for all misspelled words while performing a text check. Instead retrieve
// suggestions on demand when the context menu is brought up with a misspelled
// word selected.
BASE_DECLARE_FEATURE(kWinRetrieveSuggestionsOnlyOnDemand);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
bool IsAndroidSpellCheckFeatureEnabled();
#endif  // BUILDFLAG(IS_ANDROID)

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

}  // namespace spellcheck

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_FEATURES_H_
