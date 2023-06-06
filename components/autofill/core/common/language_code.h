// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_

#include <cctype>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/types/strong_alias.h"

namespace autofill {

// Following the implicit conventions in //components/translate, a LanguageCode
// is a lowercase alphabetic string of length up to 3, with the exception of
// "zh-CN", "zh-TW", and "mni-Mtei". A non-exhaustive list of common values is
// translate::kDefaultSupportedLanguages.
// C++ small string optimization keeps these objects lightweight so that copying
// should not be a worry.
class LanguageCode
    : public base::StrongAlias<class LanguageCodeTag, std::string> {
 private:
  using BaseClass = base::StrongAlias<LanguageCodeTag, std::string>;

 public:
  LanguageCode() = default;
  explicit LanguageCode(const char* s) : BaseClass(s) { Check(); }
  explicit LanguageCode(std::string&& s) : BaseClass(std::move(s)) { Check(); }
  explicit LanguageCode(const std::string& s) : BaseClass(s) { Check(); }

 private:
  void Check() {
    DCHECK(((*this)->size() <= 3 && base::ranges::all_of(value(), &islower)) ||
           value() == "zh-CN" || value() == "zh-TW" || value() == "mni-Mtei")
        << "Unexpected language code '" << value() << "'";
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_
