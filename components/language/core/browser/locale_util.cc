// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/locale_util.h"

#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {

std::string GetApplicationLocale(PrefService* local_state) {
  if (!local_state->HasPrefPath(prefs::kApplicationLocale))
    return std::string();
  std::string locale = local_state->GetString(prefs::kApplicationLocale);
  return l10n_util::GetApplicationLocale(locale);
}

}  // namespace language
