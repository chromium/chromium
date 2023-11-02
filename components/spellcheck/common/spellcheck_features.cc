// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spellcheck_features.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace spellcheck {

#if BUILDFLAG(ENABLE_SPELLCHECK)

bool UseBrowserSpellChecker() {
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  return false;
#elif BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(spellcheck::kWinUseBrowserSpellChecker) &&
         WindowsVersionSupportsSpellchecker();
#else
  return true;
#endif
}

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kWinUseBrowserSpellChecker,
             "WinUseBrowserSpellChecker",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWinDelaySpellcheckServiceInit,
             "WinDelaySpellcheckServiceInit",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWinRetrieveSuggestionsOnlyOnDemand,
             "WinRetrieveSuggestionsOnlyOnDemand",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool WindowsVersionSupportsSpellchecker() {
  return base::win::GetVersion() > base::win::Version::WIN7 &&
         base::win::GetVersion() < base::win::Version::WIN_LAST;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
bool IsAndroidSpellCheckFeatureEnabled() {
  return !base::SysInfo::IsLowEndDevice();
}
#endif  // BUILDFLAG(IS_ANDROID)

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

}  // namespace spellcheck
