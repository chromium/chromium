// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"

#include "base/no_destructor.h"

namespace autofill::payments {

// static
AmountExtractionHeuristicRegexes&
AmountExtractionHeuristicRegexes::GetInstance() {
  static base::NoDestructor<AmountExtractionHeuristicRegexes> instance;
  return *instance;
}

AmountExtractionHeuristicRegexes::AmountExtractionHeuristicRegexes() = default;
AmountExtractionHeuristicRegexes::~AmountExtractionHeuristicRegexes() = default;

bool AmountExtractionHeuristicRegexes::PopulateRegexFromComponent(
    const std::string& binary_pb) {
  return false;
}

std::string AmountExtractionHeuristicRegexes::GetKeywordPattern() const {
  return "";
}

std::string AmountExtractionHeuristicRegexes::GetAmountPattern() const {
  return "";
}

std::uint32_t AmountExtractionHeuristicRegexes::GetNumberOfAncestorLevels()
    const {
  return 0;
}

}  // namespace autofill::payments
