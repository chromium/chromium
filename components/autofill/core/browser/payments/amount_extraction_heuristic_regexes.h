// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_H_

#include <stdint.h>

#include <string>

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::payments {

// This holds the set of patterns that define the set of heuristic regexes used
// for amount extraction from the DOM search process. The data is read from the
// Component Updater, which fetches it periodically from component updater to
// get the most up-to-date patterns.
class AmountExtractionHeuristicRegexes final {
 public:
  static AmountExtractionHeuristicRegexes& GetInstance();

  AmountExtractionHeuristicRegexes();
  AmountExtractionHeuristicRegexes(const AmountExtractionHeuristicRegexes&) =
      delete;
  AmountExtractionHeuristicRegexes& operator=(
      const AmountExtractionHeuristicRegexes&) = delete;
  ~AmountExtractionHeuristicRegexes();

  // Updates the string from a binary proto fetched from component updater.
  // Returns `true` if the parsing process was successful, `false` otherwise.
  bool PopulateStringFromComponent(const std::string& binary_pb);

  // See comment for `keyword_pattern_`.
  const std::string& keyword_pattern() const;

  // See comment for `amount_pattern_`.
  const std::string& amount_pattern() const;

  // See comment for `number_of_ancestor_levels_to_search_`.
  uint32_t number_of_ancestor_levels_to_search() const;

  void ResetRegexStringPatternsForTesting() {
    keyword_pattern_.reset();
    amount_pattern_.reset();
    number_of_ancestor_levels_to_search_ =
        kDefaultNumberOfAncestorLevelsToSearch;
  }

 private:
  static constexpr char kDefaultKeywordPattern[] = "^(Order Total|Total):?$";
  static constexpr char kDefaultAmountPatternPattern[] =
      R"regexp((?:\$)\s*\d{1,3}(?:[.,]\d{3})*(?:[.,]\d{2})?)regexp";
  static constexpr uint32_t kDefaultNumberOfAncestorLevelsToSearch = 6;

  // A keyword pattern string used for amount extraction from DOM search
  // process. Invalidated by PopulateStringFromComponent().
  mutable std::unique_ptr<std::string> keyword_pattern_;
  // An amount pattern string used for amount extraction from DOM search
  // process. Invalidated by PopulateStringFromComponent().
  mutable std::unique_ptr<std::string> amount_pattern_;
  // The number of ancestor levels to search in the amount extraction process.
  uint32_t number_of_ancestor_levels_to_search_ =
      kDefaultNumberOfAncestorLevelsToSearch;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_H_
