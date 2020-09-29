// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

class PatternProvider {
 public:
  static PatternProvider* getInstance();

  // Setter for loaded patterns from external storage.
  void SetPatterns(
      const std::map<std::string,
                     std::map<std::string, std::vector<MatchingPattern>>>&
          patterns);

  // Provides us with all patterns that can match our field type and page
  // language.
  const std::vector<MatchingPattern>& GetMatchPatterns(
      ServerFieldType type,
      const std::string& page_language);

  const std::vector<MatchingPattern>& GetMatchPatterns(
      const std::string& pattern_name,
      const std::string& page_language);

  // Provides us with all patterns that can match our field type.
  const std::vector<MatchingPattern>& GetAllPatternsBaseOnType(
      ServerFieldType type);

 private:
  PatternProvider();
  ~PatternProvider();

  // Func to sort the incoming map by score.
  void SortPatternsByScore(std::vector<MatchingPattern>& patterns);

  // Local map to store a vector of patterns keyed by field type and
  // page language.
  std::map<std::string, std::map<std::string, std::vector<MatchingPattern>>>
      patterns_;

  friend class base::NoDestructor<PatternProvider>;
};
}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_