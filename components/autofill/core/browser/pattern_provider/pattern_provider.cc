// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/pattern_provider/pattern_provider.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {
PatternProvider::PatternProvider(autofill::ServerFieldType type) {
  autofill::MatchingPattern kCompanyPatternEn = autofill::GetCompanyPatternEn();
  autofill::MatchingPattern kCompanyPatternDe = autofill::GetCompanyPatternDe();

  patterns_[AutofillType(COMPANY_NAME).ToString()]["en"] = kCompanyPatternEn;
  patterns_[AutofillType(COMPANY_NAME).ToString()]["de"] = kCompanyPatternDe;
}

PatternProvider::~PatternProvider() {
  patterns_.clear();
}

autofill::MatchingPattern PatternProvider::GetSingleMatchPattern(
    autofill::ServerFieldType type,
    const std::string& page_language) {
  base::StringPiece type_s = autofill::FieldTypeToStringPiece(type);
  return patterns_[type_s.as_string()][page_language];
}
}  //  namespace autofill
