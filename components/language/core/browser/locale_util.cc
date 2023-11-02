// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/locale_util.h"

#include "build/build_config.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {

std::string GetApplicationLocale(PrefService* local_state) {
  std::string preferred_locale;
  // Note: This logic should match InitResourceBundleAndDetermineLocale() and
  // LoadLocaleResources(), which is how the global locale is set.
  // TODO(asvitkine): We should try to refactor things so that the logic is not
  // duplicated in multiple files.
#if !BUILDFLAG(IS_APPLE)
  // The pref isn't always registered in unit tests.
  if (local_state->HasPrefPath(prefs::kApplicationLocale))
    preferred_locale = local_state->GetString(prefs::kApplicationLocale);
#endif
  // Note: The call below is necessary even if |preferred_locale| is empty, as
  // it will get the locale that should be used potentially from other sources,
  // depending on the platform (e.g. the OS locale on Mac).
  return l10n_util::GetApplicationLocale(preferred_locale);
}

}  // namespace language
