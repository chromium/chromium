// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_H_

#include <string>

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

  // Updates the regex from a binary proto fetched from component updater.
  // Returns `true` if the parsing process was successful, `false` otherwise.
  bool PopulateRegexFromComponent(const std::string& binary_pb);

  // Returns a keyword pattern string used for amount extraction from DOM search
  // process.
  std::string GetKeywordPattern() const;

  // Returns an amount pattern string used for amount extraction from DOM search
  // process.
  std::string GetAmountPattern() const;

  // Returns the number of ancestor levels to search in the amount extraction
  // process.
  std::uint32_t GetNumberOfAncestorLevels() const;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AMOUNT_EXTRACTION_HEURISTIC_REGEXES_H_
