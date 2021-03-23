// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace base {
class FeatureList;
}

namespace variations {

class ProcessedStudy;
struct ClientFilterableState;

// Helper class to instantiate field trials from a variations seed.
class VariationsSeedProcessor {
 public:
  using UIStringOverrideCallback =
      base::RepeatingCallback<void(uint32_t, const base::string16&)>;

  VariationsSeedProcessor();
  virtual ~VariationsSeedProcessor();

  // Creates field trials from the specified variations |seed|, filtered
  // according to the client's |client_state|. Any study that should use low
  // entropy will use |low_entropy_provider| for group selection. These studies
  // are defined by ShouldStudyUseLowEntropy;
  void CreateTrialsFromSeed(
      const VariationsSeed& seed,
      const ClientFilterableState& client_state,
      const UIStringOverrideCallback& override_callback,
      const base::FieldTrial::EntropyProvider* low_entropy_provider,
      base::FeatureList* feature_list);

  // If the given |study| should alwoys use low entropy. This is true for any
  // study that can send data to other Google properties.
  static bool ShouldStudyUseLowEntropy(const Study& study);

 private:
  friend class VariationsSeedProcessorTest;
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           AllowForceGroupAndVariationId);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           AllowVariationIdWithForcingFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForbidForceGroupWithVariationId);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ForceGroupWithFlag1);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, ForceGroupWithFlag2);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForceGroup_ChooseFirstGroupWithFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           ForceGroup_DontChooseGroupWithFlag);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, IsStudyExpired);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest, VariationParams);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedProcessorTest,
                           VariationParamsWithForcingFlag);

  // Check if the |study| is only associated with platform Android/iOS and
  // channel dev/canary. If so, forcing flag and variation id can both be set.
  // (Otherwise, forcing_flag and variation_id are mutually exclusive.)
  bool AllowVariationIdWithForcingFlag(const Study& study);

  // Creates and registers a field trial from the |processed_study| data.
  // Disables the trial if |processed_study.is_expired| is true. Uses
  // |low_entropy_provider| if ShouldStudyUseLowEntropy returns true for the
  // study.
  void CreateTrialFromStudy(
      const ProcessedStudy& processed_study,
      const UIStringOverrideCallback& override_callback,
      const base::FieldTrial::EntropyProvider* low_entropy_provider,
      base::FeatureList* feature_list);

  DISALLOW_COPY_AND_ASSIGN(VariationsSeedProcessor);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
