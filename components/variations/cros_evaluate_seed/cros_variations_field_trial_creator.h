// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_CROS_VARIATIONS_FIELD_TRIAL_CREATOR_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_CROS_VARIATIONS_FIELD_TRIAL_CREATOR_H_

#include "components/variations/service/variations_field_trial_creator_base.h"

namespace variations {

class VariationsSeedStore;
class VariationsServiceClient;

namespace cros_early_boot::evaluate_seed {

// Used to set up field trials for early-boot ChromeOS experiments based on
// stored variations seed data.
class CrOSVariationsFieldTrialCreator : public VariationsFieldTrialCreatorBase {
 public:
  // Caller is responsible for ensuring that the VariationsServiceClient passed
  // to the constructor stays valid for the lifetime of this object.
  CrOSVariationsFieldTrialCreator(
      VariationsServiceClient* client,
      std::unique_ptr<VariationsSeedStore> seed_store);

  CrOSVariationsFieldTrialCreator(const CrOSVariationsFieldTrialCreator&) =
      delete;
  CrOSVariationsFieldTrialCreator& operator=(
      const CrOSVariationsFieldTrialCreator&) = delete;

  ~CrOSVariationsFieldTrialCreator() override;

  // We do not support overriding UI strings, so override with no-op
  // implementations.
  void OverrideCachedUIStrings() override {}
  bool IsOverrideResourceMapEmpty() override;

 protected:
  // We do not support overriding UI strings, so override with no-op
  // implementation.
  void OverrideUIString(uint32_t hash, const std::u16string& str) override {}
};

}  // namespace cros_early_boot::evaluate_seed
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_CROS_VARIATIONS_FIELD_TRIAL_CREATOR_H_
