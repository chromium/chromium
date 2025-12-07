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

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
bool IsAndroidSpellCheckFeatureEnabled();

BASE_DECLARE_FEATURE(kAndroidGrammarCheck);
#endif  // BUILDFLAG(IS_ANDROID)

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

}  // namespace spellcheck

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_FEATURES_H_
