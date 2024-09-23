// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace base {
class FeatureList;
}

namespace variations {

namespace internal {
// The trial group selected when a study specifies a feature that is already
// associated with another trial. Exposed in the header file for testing.
COMPONENT_EXPORT(VARIATIONS)
extern const char kFeatureConflictGroupName[];

// The name of an auto-generated feature parameter for studies that have a
// non-empty google_groups filter.
COMPONENT_EXPORT(VARIATIONS)
extern const char kGoogleGroupFeatureParamName[];

// The separator between multiple Google groups in when serialized into a string
// for the feature parameter.
COMPONENT_EXPORT(VARIATIONS)
extern const char kGoogleGroupFeatureParamSeparator[];
}  // namespace internal

class ProcessedStudy;
struct ClientFilterableState;
class VariationsLayers;

// Helper class to instantiate field trials from a variations seed.
class COMPONENT_EXPORT(VARIATIONS) VariationsSeedProcessor {
 public:
  using UIStringOverrideCallback =
      base::RepeatingCallback<void(uint32_t, const std::u16string&)>;

  VariationsSeedProcessor();

  VariationsSeedProcessor(const VariationsSeedProcessor&) = delete;
  VariationsSeedProcessor& operator=(const VariationsSeedProcessor&) = delete;

  virtual ~VariationsSeedProcessor();

  // Whether the experiment has a `google_web_experiment_id` or a
  // `google_web_trigger_experiment_id`.
  static bool HasGoogleWebExperimentId(const Study::Experiment& experiment);

  // Creates field trials from the specified variations |seed|, filtered
  // according to the client's |client_state|.
  void CreateTrialsFromSeed(const VariationsSeed& seed,
                            const ClientFilterableState& client_state,
                            const UIStringOverrideCallback& override_callback,
                            const EntropyProviders& entropy_providers,
                            const VariationsLayers& layers,
                            base::FeatureList* feature_list);

 private:
  friend void CreateTrialFromStudyFuzzer(const Study& study);

  // Check if the |study| is only associated with platform Android/iOS and
  // channel dev/canary. If so, forcing flag and variation id can both be set.
  // (Otherwise, forcing_flag and variation_id are mutually exclusive.)
  bool AllowVariationIdWithForcingFlag(const Study& study);

  // Creates and registers a field trial from the |processed_study| data.
  void CreateTrialFromStudy(const ProcessedStudy& processed_study,
                            const UIStringOverrideCallback& override_callback,
                            const EntropyProviders& entropy_providers,
                            const VariationsLayers& layers,
                            base::FeatureList* feature_list);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_PROCESSOR_H_
