// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_DETERMINE_HEURISTIC_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_DETERMINE_HEURISTIC_TYPES_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;
class FormStructure;
class LogManager;

// Holds the predictions returned by DetermineHeuristicTypes().
class HeuristicPredictions {
 public:
  HeuristicPredictions(HeuristicSource source,
                       const FieldCandidatesMap& field_type_map,
                       base::span<const std::unique_ptr<AutofillField>> fields);
  HeuristicPredictions(const HeuristicPredictions&);
  HeuristicPredictions(HeuristicPredictions&&);
  HeuristicPredictions& operator=(const HeuristicPredictions&);
  HeuristicPredictions& operator=(HeuristicPredictions&&);
  ~HeuristicPredictions();

  // Sets the heuristic types of `fields` according to `this`.
  void ApplyTo(base::span<const std::unique_ptr<AutofillField>> fields) const;

 private:
  HeuristicSource source_ = internal::IsRequired();
  base::flat_map<FieldGlobalId, FieldType> predictions_;
};

// Runs several heuristics against the form fields to determine their possible
// types.
[[nodiscard]] HeuristicPredictions DetermineHeuristicTypes(
    const GeoIpCountryCode& client_country,
    const LanguageCode& current_page_language,
    FormStructure& form,
    LogManager* log_manager);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_DETERMINE_HEURISTIC_TYPES_H_
