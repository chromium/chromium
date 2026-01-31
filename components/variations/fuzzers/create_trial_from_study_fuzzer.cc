// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace variations {
namespace {

struct Environment {
  Environment() { base::CommandLine::Init(0, nullptr); }

  base::AtExitManager at_exit_manager;
};

}  // namespace

void CreateTrialFromStudyFuzzer(const Study& study) {
  base::FieldTrialList field_trial_list;
  base::FeatureList feature_list;

  EntropyProviders entropy_providers(
      "client_id", {7999, 8000},
      // Test value for limited entropy randomization source.
      "00000000000000000000000000000001");
  ProcessedStudy processed_study;
  VariationsLayers layers;
  if (processed_study.Init(&study)) {
    StickyActivationManager sticky_activation_manager(/*local_state=*/nullptr);
    VariationsSeedProcessor(sticky_activation_manager)
        .CreateTrialFromStudy(processed_study, entropy_providers, layers,
                              &feature_list);
  }
}

DEFINE_PROTO_FUZZER(const Study& study) {
  static Environment env;
  CreateTrialFromStudyFuzzer(study);
}

}  // namespace variations
