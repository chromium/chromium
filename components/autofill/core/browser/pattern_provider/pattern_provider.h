// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_

#include <string>
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

class PatternProvider {
 public:
  PatternProvider();
  PatternProvider(ServerFieldType type, const std::string& page_language);
  explicit PatternProvider(ServerFieldType type);
  ~PatternProvider();

  // Provides us with all patterns that can match our field type and page
  // language.
  const std::vector<MatchingPattern>& GetMatchPatterns(
      ServerFieldType type,
      const std::string& page_language);

  const std::vector<MatchingPattern>& GetMatchPatterns(
      const std::string& pattern_name,
      const std::string& page_launguage);

  // Provides us with all patterns that can match our field type.
  const std::vector<MatchingPattern>& GetAllPatternsBaseOnType(
      ServerFieldType type);

  // Function that returns pattern that match our field type and page language.
  MatchingPattern GetSingleMatchPattern(ServerFieldType type,
                                        const std::string& page_language);

 private:
  // Func to sort the incoming map by score.
  void SortPatternsByScore(std::vector<MatchingPattern>& patterns);

  // Local map to store patterns keyed by field type and page language.
  std::map<std::string, std::map<std::string, MatchingPattern>> patterns_;
};
}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PATTERN_PROVIDER_PATTERN_PROVIDER_H_