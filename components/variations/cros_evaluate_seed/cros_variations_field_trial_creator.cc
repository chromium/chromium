// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/cros_variations_field_trial_creator.h"

// Determining locale correctly is expensive in terms of code size due to the
// libicu dependency (~400 KiB). Since it is not critical for early-boot
// features to know locale, use a fake one that will *never* match.
constexpr char kFakeLocale[] = "invalid-locale";

class PrefService;

namespace variations::cros_early_boot::evaluate_seed {

CrOSVariationsFieldTrialCreator::CrOSVariationsFieldTrialCreator(
    VariationsServiceClient* client,
    std::unique_ptr<VariationsSeedStore> seed_store)
    : VariationsFieldTrialCreatorBase(
          client,
          std::move(seed_store),
          base::BindOnce([](PrefService*) { return std::string(kFakeLocale); }),
          // The limited entropy synthetic trial will not be registered for this
          // purpose.
          /*limited_entropy_synthetic_trial=*/nullptr) {}

CrOSVariationsFieldTrialCreator::~CrOSVariationsFieldTrialCreator() = default;

bool CrOSVariationsFieldTrialCreator::IsOverrideResourceMapEmpty() {
  return true;
}

}  // namespace variations::cros_early_boot::evaluate_seed
