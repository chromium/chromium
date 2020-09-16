// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_PARSING_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_AUTOFILL_PARSING_UTILS_H_

#include <string>

namespace autofill {

// A bit-field used for matching specific parts of a field in question.
// Attributes.
enum MatchAttributes {
  MATCH_LABEL = 1 << 0,
  MATCH_NAME = 1 << 1,
  MATCH_ATTRIBUTES_DEFAULT = MATCH_LABEL | MATCH_NAME,
};

// A bit-field used for matching specific parts of a field in question.
// Input types.
enum MatchFieldTypes {
  MATCH_TEXT = 1 << 2,
  MATCH_EMAIL = 1 << 3,
  MATCH_TELEPHONE = 1 << 4,
  MATCH_SELECT = 1 << 5,
  MATCH_TEXT_AREA = 1 << 6,
  MATCH_PASSWORD = 1 << 7,
  MATCH_NUMBER = 1 << 8,
  MATCH_SEARCH = 1 << 9,
  MATCH_ALL_INPUTS = MATCH_TEXT | MATCH_EMAIL | MATCH_TELEPHONE | MATCH_SELECT |
                     MATCH_TEXT_AREA | MATCH_PASSWORD | MATCH_NUMBER |
                     MATCH_SEARCH,

  // By default match label and name for input/text types.
  MATCH_INPUTS_DEFAULT = MATCH_TEXT,
};

constexpr int MATCH_DEFAULT = MATCH_ATTRIBUTES_DEFAULT | MATCH_INPUTS_DEFAULT;

// Structure for a better organization of data and regular expressions
// for autofill regex_constants. In the future, to implement faster
// changes without global updates also for having a quick possibility
// to recognize incorrect matches.
struct MatchingPattern {
  MatchingPattern();
  MatchingPattern(const MatchingPattern& mp);
  MatchingPattern& operator=(const MatchingPattern& mp);
  ~MatchingPattern();

  std::string pattern_identifier;
  std::string positive_pattern;
  float positive_score = 1.1f;
  std::string negative_pattern;
  int match_field_attributes;
  int match_field_input_types;
  std::string language;
};

// Use these functions instead of storing "non standats type" constants that
// bots might complaining over.
MatchingPattern GetCompanyPatternEn();
MatchingPattern GetCompanyPatternDe();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_PARSING_UTILS_H_
