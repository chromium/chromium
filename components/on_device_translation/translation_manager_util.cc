// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/translation_manager_util.h"

#include <algorithm>

#include "base/i18n/legacy_language_tag_helpers.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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

namespace {

// TODO(crbug.com/419848973): This is a workaround until the "he" language code
// is fully supported.
std::string SwitchLanguageCodeToIwIfHe(std::string_view language_code) {
  if (language_code == "he") {
    return "iw";
  }
  auto split = base::SplitStringOnce(language_code, "-");
  if (!split || split->first != "he") {
    return std::string(language_code);
  }

  return base::StrCat({"iw-", split->second});
}

std::string SwitchLanguageCodeForChinese(std::string_view language_code) {
  std::string lower_code = base::ToLowerASCII(language_code);
  if (lower_code == "zh-cn" || lower_code == "zh-hans-cn" ||
      lower_code == "cmn-hans-cn") {
    return "zh";
  }
  if (lower_code == "zh-tw" || lower_code == "zh-hant-tw" ||
      lower_code == "cmn-hant-tw" || lower_code == "zh-hk" ||
      lower_code == "zh-hant-hk" || lower_code == "cmn-hant-hk") {
    return "zh-Hant";
  }
  return std::string(language_code);
}

}  // namespace

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
  return std::ranges::contains(
      accept_languages, base::i18n::GetLanguageSubtagUsingLanguageTag(lang));
}

bool IsTranslatorAllowed(content::BrowserContext* browser_context) {
  CHECK(browser_context);
  return user_prefs::UserPrefs::Get(browser_context)
      ->GetBoolean(prefs::kTranslatorAPIAllowed);
}

std::optional<std::string> GetBestFitLanguageCode(
    std::string_view language_code) {
  std::string best_fit = SwitchLanguageCodeToIwIfHe(language_code);
  best_fit = SwitchLanguageCodeForChinese(best_fit);
  return LookupMatchingLocaleByBestFit(kSupportedLanguageCodes, best_fit);
}

}  // namespace on_device_translation
