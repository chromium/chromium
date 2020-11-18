// Copyright 2020 The Chromium Authors. All rights reserved.
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

// A LanguageCode is a two-letter lowercase abbreviation according to ISO 639-1
// or "und", which is the ISO 639-2 code for "undetermined".
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
    DCHECK(empty() || length() == 2 || value() == "und")
        << "Invalid language code '" << value() << "'";
    DCHECK(base::ranges::all_of(value(), &islower))
        << "Invalid language code '" << value() << "'";
  }
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_LANGUAGE_CODE_H_
