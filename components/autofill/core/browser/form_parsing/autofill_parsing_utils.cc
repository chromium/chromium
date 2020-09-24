// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"

namespace autofill {

MatchingPattern::MatchingPattern() = default;
MatchingPattern::MatchingPattern(const MatchingPattern& mp) = default;
MatchingPattern& MatchingPattern::operator=(const MatchingPattern& mp) =
    default;

MatchingPattern::~MatchingPattern() = default;

autofill::MatchingPattern GetCompanyPatternEn() {
  autofill::MatchingPattern m_p;
  m_p.pattern_identifier = "kCompanyPatternEn";
  m_p.positive_pattern = "company|business|organization|organisation";
  m_p.positive_score = 1.1f;
  m_p.negative_pattern = "";
  m_p.match_field_attributes = MATCH_NAME;
  m_p.match_field_input_types = MATCH_TEXT;
  m_p.language = "en";

  return m_p;
}

autofill::MatchingPattern GetCompanyPatternDe() {
  autofill::MatchingPattern m_p;

  m_p.pattern_identifier = "kCompanyPatternDe";
  m_p.positive_pattern = "|(?<!con)firma|firmenname";
  m_p.positive_score = 1.1f;
  m_p.negative_pattern = "";
  m_p.match_field_attributes = MATCH_LABEL | MATCH_NAME;
  m_p.match_field_input_types = MATCH_TEXT;
  m_p.language = "de";

  return m_p;
}

}  // namespace autofill