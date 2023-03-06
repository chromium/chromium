// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_optimization_guide.h"

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace autofill {

AutofillOptimizationGuide::AutofillOptimizationGuide(
    optimization_guide::NewOptimizationGuideDecider* decider)
    : decider_(decider) {}

AutofillOptimizationGuide::~AutofillOptimizationGuide() = default;

void AutofillOptimizationGuide::OnDidParseForm(
    const FormStructure& form_structure) {
  // This flat set represents all of the optimization types that we need to
  // register based on `form_structure`.
  base::flat_set<optimization_guide::proto::OptimizationType>
      optimization_types;

  for (const auto& field : form_structure) {
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableIbanClientSideUrlFiltering) &&
        field->Type().GetStorableType() == IBAN_VALUE) {
      optimization_types.insert(
          optimization_guide::proto::IBAN_AUTOFILL_BLOCKED);
    }
  }

  // If we do not have any optimization types to register, do not do anything.
  if (!optimization_types.empty()) {
    // Register all optimization types that we need based on `form_structure`.
    decider_->RegisterOptimizationTypes(
        std::vector<optimization_guide::proto::OptimizationType>(
            std::move(optimization_types).extract()));
  }
}

bool AutofillOptimizationGuide::ShouldBlockSingleFieldSuggestions(
    const GURL& url,
    AutofillField* field) const {
  // If the field's storable type is `IBAN_VALUE`, check whether IBAN
  // suggestions should be blocked based on `url`.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableIbanClientSideUrlFiltering) &&
      field->Type().GetStorableType() == IBAN_VALUE) {
    optimization_guide::OptimizationGuideDecision decision =
        decider_->CanApplyOptimization(
            url, optimization_guide::proto::IBAN_AUTOFILL_BLOCKED,
            /*optimization_metadata=*/nullptr);
    // Since the optimization guide decider integration corresponding to
    // `optimization_guide::proto::IBAN_AUTOFILL_BLOCKED` is a blocklist,
    // `optimization_guide::OptimizationGuideDecision::kFalse` indicates that
    // `url` is blocked from displaying IBAN suggestions. If the optimization
    // type was not registered in time when we queried it, it will be
    // `kUnknown`, so the default functionality in this case will be to not
    // block the suggestions from being shown.
    return decision == optimization_guide::OptimizationGuideDecision::kFalse;
  }

  // No conditions indicating single field suggestions should be blocked were
  // encountered, so return that they should not be blocked.
  return false;
}

}  // namespace autofill
