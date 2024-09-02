// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_TEST_API_H_

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"

namespace autofill {

class MatchPatternRefTestApi {
 public:
  using UnderlyingType = MatchPatternRef::UnderlyingType;

  explicit MatchPatternRefTestApi(MatchPatternRef p) : p_(p) {}

  std::optional<MatchPatternRef> MakeSupplementary() const {
    if (!(*p_).match_field_attributes.contains(MatchAttribute::kName)) {
      return std::nullopt;
    }
    return MatchPatternRef(true, index());
  }

  UnderlyingType is_supplementary() const { return p_.is_supplementary(); }

  UnderlyingType index() const { return p_.index(); }

 private:
  MatchPatternRef p_;
};

MatchPatternRefTestApi test_api(MatchPatternRef p) {
  return MatchPatternRefTestApi(p);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_REGEX_PATTERNS_TEST_API_H_
