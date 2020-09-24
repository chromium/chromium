// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {
PatternProvider::PatternProvider() {
  auto& company_patterns = patterns_[AutofillType(COMPANY_NAME).ToString()];
  company_patterns["EN"].push_back(GetCompanyPatternEn());
  company_patterns["DE"].push_back(GetCompanyPatternDe());
}

PatternProvider::~PatternProvider() {
  patterns_.clear();
}

void PatternProvider::SetPatterns(
    const std::map<std::string,
                   std::map<std::string, std::vector<MatchingPattern>>>&
        patterns) {
  patterns_ = patterns;
}

const std::vector<MatchingPattern>& PatternProvider::GetMatchPatterns(
    const std::string& pattern_name,
    const std::string& page_language) {
  return patterns_[pattern_name][page_language];
}

const std::vector<MatchingPattern>& PatternProvider::GetMatchPatterns(
    ServerFieldType type,
    const std::string& page_language) {
  std::string pattern_name = AutofillType(type).ToString();
  return GetMatchPatterns(pattern_name, page_language);
}

PatternProvider* PatternProvider::getInstance() {
  static base::NoDestructor<PatternProvider> instance;
  return instance.get();
}

}  //  namespace autofill
