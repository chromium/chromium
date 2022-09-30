// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/locale_util.h"

#include <stddef.h>

#include "base/ranges/algorithm.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {

std::pair<base::StringPiece, base::StringPiece> SplitIntoMainAndTail(
    base::StringPiece locale) {
  size_t hyphen_pos =
      static_cast<size_t>(base::ranges::find(locale, '-') - locale.begin());
  return std::make_pair(locale.substr(0U, hyphen_pos),
                        locale.substr(hyphen_pos));
}

base::StringPiece ExtractBaseLanguage(base::StringPiece language_code) {
  return SplitIntoMainAndTail(language_code).first;
}

bool ConvertToActualUILocale(std::string* input_locale) {
  std::string original_locale;
  input_locale->swap(original_locale);
  return l10n_util::CheckAndResolveLocale(original_locale, input_locale,
                                          /*perform_io=*/false);
}

}  // namespace language
