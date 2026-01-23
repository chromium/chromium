// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/translation_manager_util.h"

#include <algorithm>

#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "components/language/core/browser/pref_names.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/l10n/l10n_util.h"

namespace on_device_translation {

const std::vector<std::string_view> GetAcceptLanguages(
    content::BrowserContext* browser_context) {
  CHECK(browser_context);

  PrefService* profile_pref = user_prefs::UserPrefs::Get(browser_context);
  const std::vector<std::string_view> accept_languages = base::SplitStringPiece(
      profile_pref->GetString(language::prefs::kAcceptLanguages), ",",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return accept_languages;
}

bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string_view lang) {
  return std::ranges::contains(accept_languages, l10n_util::GetLanguage(lang));
}

bool IsTranslatorAllowed(content::BrowserContext* browser_context) {
  CHECK(browser_context);
  return user_prefs::UserPrefs::Get(browser_context)
      ->GetBoolean(prefs::kTranslatorAPIAllowed);
}

}  // namespace on_device_translation
