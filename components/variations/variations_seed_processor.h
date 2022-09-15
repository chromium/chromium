// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace base {
class FeatureList;
}

namespace variations {

namespace internal {
// The trial group selected when a study specifies a feature that is already
// associated with another trial. Exposed in the header file for testing.
COMPONENT_EXPORT(VARIATIONS) extern const char kFeatureConflictGroupName[];
}  // namespace internal

class ProcessedStudy;
struct ClientFilterableState;

// Helper class to instantiate field trials from a variations seed.
class COMPONENT_EXPORT(VARIATIONS) VariationsSeedProcessor {
 public:
  using UIStringOverrideCallback =
      base::RepeatingCallback<void(uint32_t, const std::u16string&)>;

  VariationsSeedProcessor();

  VariationsSeedProcessor(const VariationsSeedProcessor&) = delete;
  VariationsSeedProcessor& operator=(const VariationsSeedProcessor&) = delete;

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
  friend void CreateTrialFromStudyFuzzer(const Study& study);

  // Check if the |study| is only associated with platform Android/iOS and
  // channel dev/canary. If so, forcing flag and variation id can both be set.
  // (Otherwise, forcing_flag and variation_id are mutually exclusive.)
  bool AllowVariationIdWithForcingFlag(const Study& study);

  // Creates and registers a field trial from the |processed_study| data. Uses
  // |low_entropy_provider| if ShouldStudyUseLowEntropy returns true for the
  // study.
  void CreateTrialFromStudy(
      const ProcessedStudy& processed_study,
      const UIStringOverrideCallback& override_callback,
      const base::FieldTrial::EntropyProvider* low_entropy_provider,
      base::FeatureList* feature_list);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
