// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_FIELD_TRIALS_PROVIDER_HELPER_H_
#define COMPONENTS_UKM_FIELD_TRIALS_PROVIDER_HELPER_H_

#include <memory>

#include "components/metrics/field_trials_provider.h"

namespace variations {
class SyntheticTrialRegistry;
}

namespace ukm {

// Creates a FieldTrialsProvider for use with UKMs.
std::unique_ptr<variations::FieldTrialsProvider>
CreateFieldTrialsProviderForUkm(
    variations::SyntheticTrialRegistry* synthetic_field_trial_registry);

}  // namespace ukm

#endif  // COMPONENTS_UKM_FIELD_TRIALS_PROVIDER_HELPER_H_
