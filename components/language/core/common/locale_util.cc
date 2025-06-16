// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/locale_util.h"

#include <stddef.h>

#include <algorithm>
#include <string_view>

#include "ui/base/l10n/l10n_util.h"

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
  if (std::optional<std::string> resolved_locale =
          l10n_util::CheckAndResolveLocale(
              *input_locale,
              l10n_util::CheckLocaleMode::kUseKnownLocalesList)) {
    input_locale->swap(*resolved_locale);
    return true;
  }
  return false;
}

}  // namespace language
