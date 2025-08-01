// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

namespace variations {

VariationsFieldTrialCreator::VariationsFieldTrialCreator(
    VariationsServiceClient* client,
    std::unique_ptr<VariationsSeedStore> seed_store,
    const UIStringOverrider& ui_string_overrider)
    : VariationsFieldTrialCreatorBase(client,
                                      std::move(seed_store),
                                      ui_string_overrider) {}

VariationsFieldTrialCreator::~VariationsFieldTrialCreator() = default;

}  // namespace variations
