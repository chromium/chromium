// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.pb.h"

using ::autofill::core::browser::payments::HeuristicRegexes;
using ::autofill::core::browser::payments::HeuristicRegexes_GenericDetails;

namespace autofill::payments {

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
  HeuristicRegexes heuristic_regexes;
  if (!heuristic_regexes.ParseFromString(binary_pb)) {
    autofill::autofill_metrics::LogAmountExtractionComponentInstallationResult(
        autofill::autofill_metrics::
            AmountExtractionComponentInstallationResult::kParsingToProtoFailed);
    return false;
  }

  if (!heuristic_regexes.has_generic_details()) {
    autofill::autofill_metrics::LogAmountExtractionComponentInstallationResult(
        autofill::autofill_metrics::
            AmountExtractionComponentInstallationResult::kEmptyGenericDetails);
    return false;
  }

  std::unique_ptr<HeuristicRegexes_GenericDetails> details =
      base::WrapUnique(heuristic_regexes.release_generic_details());
  keyword_pattern_ = base::WrapUnique(details->release_keyword_pattern());
  amount_pattern_ = base::WrapUnique(details->release_amount_pattern());
  number_of_ancestor_levels_to_search_ =
      details->number_of_ancestor_levels_to_search();

  autofill::autofill_metrics::LogAmountExtractionComponentInstallationResult(
      autofill::autofill_metrics::AmountExtractionComponentInstallationResult::
          kSuccessful);
  return true;
}

const std::string& AmountExtractionHeuristicRegexes::keyword_pattern() const {
  if (!keyword_pattern_) {
    keyword_pattern_ = std::make_unique<std::string>(kDefaultKeywordPattern);
  }
  return *keyword_pattern_;
}

const std::string& AmountExtractionHeuristicRegexes::amount_pattern() const {
  if (!amount_pattern_) {
    amount_pattern_ =
        std::make_unique<std::string>(kDefaultAmountPatternPattern);
  }
  return *amount_pattern_;
}

uint32_t AmountExtractionHeuristicRegexes::number_of_ancestor_levels_to_search()
    const {
  return number_of_ancestor_levels_to_search_;
}

}  // namespace autofill::payments
