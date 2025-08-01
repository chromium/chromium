// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "components/variations/service/ui_string_overrider.h"
#include "components/variations/service/variations_field_trial_creator_base.h"
#include "components/variations/variations_seed_store.h"

namespace variations {

class VariationsServiceClient;

// Used to set up field trials based on stored variations seed data.
class VariationsFieldTrialCreator : public VariationsFieldTrialCreatorBase {
 public:
  // Caller is responsible for ensuring that objects passed to the constructor
  // stay valid for the lifetime of this object.
  VariationsFieldTrialCreator(VariationsServiceClient* client,
                              std::unique_ptr<VariationsSeedStore> seed_store,
                              const UIStringOverrider& ui_string_overrider);

  VariationsFieldTrialCreator(const VariationsFieldTrialCreator&) = delete;
  VariationsFieldTrialCreator& operator=(const VariationsFieldTrialCreator&) =
      delete;

  ~VariationsFieldTrialCreator() override;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_FIELD_TRIAL_CREATOR_H_
