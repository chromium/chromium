// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_

#include <cctype>
#include <string>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/types/strong_alias.h"

namespace autofill {

// Following the implicit conventions in //components/translate, a LanguageCode
// in  is a lowercase alphabetic string of length up to 3, or "zh-CN", or
// "zh-TW". A non-exhaustive list of common values is
// translate::kDefaultSupportedLanguages.
class LanguageCode
    : public base::StrongAlias<class LanguageCodeTag, std::string> {
 private:
  using BaseClass = base::StrongAlias<LanguageCodeTag, std::string>;

 public:
  LanguageCode() = default;
  explicit LanguageCode(const char* s) : BaseClass(s) { Check(); }
  explicit LanguageCode(std::string&& s) : BaseClass(std::move(s)) { Check(); }
  explicit LanguageCode(const std::string& s) : BaseClass(s) { Check(); }

  size_t length() const { return value().length(); }
  bool empty() const { return value().empty(); }

 private:
  void Check() {
    DCHECK((length() <= 3 && base::ranges::all_of(value(), &islower)) ||
           value() == "zh-CN" || value() == "zh-TW")
        << "Unexpected language code '" << value() << "'";
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_
