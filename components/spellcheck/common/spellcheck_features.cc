// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spellcheck_features.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace spellcheck {

#if BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_WIN)
namespace {
// The browser spell checker may be disabled in tests.
bool g_browser_spell_checker_enabled = true;
}  // namespace
#endif  // BUILDFLAG(IS_WIN)

bool UseBrowserSpellChecker() {
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  return false;
#elif BUILDFLAG(IS_WIN)
  return g_browser_spell_checker_enabled;
#else
  return true;
#endif
}

#if BUILDFLAG(IS_WIN)
ScopedDisableBrowserSpellCheckerForTesting::
    ScopedDisableBrowserSpellCheckerForTesting()
    : previous_value_(g_browser_spell_checker_enabled) {
  g_browser_spell_checker_enabled = false;
}

ScopedDisableBrowserSpellCheckerForTesting::
    ~ScopedDisableBrowserSpellCheckerForTesting() {
  g_browser_spell_checker_enabled = previous_value_;
}

BASE_FEATURE(kWinDelaySpellcheckServiceInit,
             "WinDelaySpellcheckServiceInit",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWinRetrieveSuggestionsOnlyOnDemand,
             "WinRetrieveSuggestionsOnlyOnDemand",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
bool IsAndroidSpellCheckFeatureEnabled() {
  return !base::SysInfo::IsLowEndDevice();
}
#endif  // BUILDFLAG(IS_ANDROID)

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

}  // namespace spellcheck
