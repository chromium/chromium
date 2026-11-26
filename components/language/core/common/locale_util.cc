// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/locale_util.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"
#include "base/i18n/tag_converters.h"
#include "ui/base/l10n/chromium_language_matcher.h"

namespace language {

std::pair<std::string_view, std::string_view> SplitIntoMainAndTail(
    std::string_view locale) {
  size_t hyphen_pos =
      static_cast<size_t>(std::ranges::find(locale, '-') - locale.begin());
  return std::make_pair(locale.substr(0U, hyphen_pos),
                        locale.substr(hyphen_pos));
}

std::string_view ExtractBaseLanguage(std::string_view language_code) {
  return SplitIntoMainAndTail(language_code).first;
}

bool ConvertToActualUILocale(std::string* input_locale) {
  std::optional<base::i18n::LanguageTag> matched =
      base::i18n::GetLanguageTagFromString(*input_locale)
          .and_then([](const base::i18n::LanguageTag& locale) {
            return ui_l10n::GetPlatformLanguageMatcher().Match(locale);
          });
  if (!matched) {
    return false;
  }
  *input_locale = std::string(matched->tag_string());
  return true;
}

}  // namespace language
