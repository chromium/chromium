// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.pb.h"

using ::autofill::core::browser::payments::HeuristicRegexes;

namespace autofill::payments {

namespace {
const char* kDefaultKeywordPattern = "^(Order Total|Total):?$";
const char* kDefaultAmountPatternPattern =
    R"regexp((?:\$)\s*\d{1,3}(?:[.,]\d{3})*(?:[.,]\d{2})?)regexp";
}  // namespace

// static
AmountExtractionHeuristicRegexes&
AmountExtractionHeuristicRegexes::GetInstance() {
  static base::NoDestructor<AmountExtractionHeuristicRegexes> instance;
  return *instance;
}

AmountExtractionHeuristicRegexes::AmountExtractionHeuristicRegexes() = default;
AmountExtractionHeuristicRegexes::~AmountExtractionHeuristicRegexes() = default;

bool AmountExtractionHeuristicRegexes::PopulateStringFromComponent(
    const std::string& binary_pb) {
  if (binary_pb.empty()) {
    return false;
  }

  HeuristicRegexes heuristic_regexes;
  if (!heuristic_regexes.ParseFromString(binary_pb)) {
    return false;
  }

  if (!heuristic_regexes.has_generic_details()) {
    return false;
  }

  keyword_pattern_ = heuristic_regexes.generic_details().keyword_pattern();
  amount_pattern_ = heuristic_regexes.generic_details().amount_pattern();
  number_of_ancestor_levels_to_search_ =
      heuristic_regexes.generic_details().number_of_ancestor_levels_to_search();

  return true;
}

std::string AmountExtractionHeuristicRegexes::keyword_pattern() const {
  if (keyword_pattern_.empty()) {
    return kDefaultKeywordPattern;
  }
  return keyword_pattern_;
}

std::string AmountExtractionHeuristicRegexes::amount_pattern() const {
  if (amount_pattern_.empty()) {
    return kDefaultAmountPatternPattern;
  }
  return amount_pattern_;
}

uint32_t AmountExtractionHeuristicRegexes::number_of_ancestor_levels_to_search()
    const {
  return number_of_ancestor_levels_to_search_;
}

}  // namespace autofill::payments
