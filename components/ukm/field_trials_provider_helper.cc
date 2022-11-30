// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/field_trials_provider_helper.h"

namespace ukm {

namespace {

// UKM suffix for field trial recording.
const char kUKMFieldTrialSuffix[] = "UKM";

}  // namespace

std::unique_ptr<variations::FieldTrialsProvider>
CreateFieldTrialsProviderForUkm(
    variations::SyntheticTrialRegistry* synthetic_trial_registry) {
  return std::make_unique<variations::FieldTrialsProvider>(
      synthetic_trial_registry, kUKMFieldTrialSuffix);
}

}  //  namespace ukm
