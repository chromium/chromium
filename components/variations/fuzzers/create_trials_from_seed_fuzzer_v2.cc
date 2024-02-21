// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/fuzzers/create_trials_from_seed_test_case.pb.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_test_utils.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace variations {
namespace {

struct Environment {
  Environment() { base::CommandLine::Init(0, nullptr); }

  base::AtExitManager at_exit_manager;
};

EntropyProviders CreateEntropyProviders(
    const CreateTrialsFromSeedTestCase::EntropyValues& entropy_values) {
  return EntropyProviders(
      entropy_values.client_id(), {entropy_values.low_entropy(), 8000},
      entropy_values.limited_entropy_randomization_source());
}

std::unique_ptr<ClientFilterableState> CreateClientFilterableState(
    const CreateTrialsFromSeedTestCase::ClientFilterableState& spec) {
  auto client_state = std::make_unique<ClientFilterableState>(
      base::BindOnce([] { return false; }),
      base::BindLambdaForTesting([spec]() {
        return base::flat_set<uint64_t>(spec.google_groups().begin(),
                                        spec.google_groups().end());
      }));

  if (spec.has_locale()) {
    client_state->locale = spec.locale();
  }
  if (spec.has_reference_date_seconds_since_epoch()) {
    client_state->reference_date =
        base::Time::UnixEpoch() +
        base::Seconds(spec.reference_date_seconds_since_epoch());
  }
  if (!spec.version().empty()) {
    client_state->version = base::Version(
        std::vector<uint32_t>(spec.version().begin(), spec.version().end()));
  }
  if (!spec.os_version().empty()) {
    client_state->os_version = base::Version(std::vector<uint32_t>(
        spec.os_version().begin(), spec.os_version().end()));
  }
  if (spec.has_channel()) {
    client_state->channel = spec.channel();
  }
  if (spec.has_form_factor()) {
    client_state->form_factor = spec.form_factor();
  }
  if (spec.has_cpu_architecture()) {
    client_state->cpu_architecture = spec.cpu_architecture();
  }
  if (spec.has_platform()) {
    client_state->platform = spec.platform();
  }
  if (spec.has_hardware_class()) {
    client_state->hardware_class = spec.hardware_class();
  }
  if (spec.has_is_low_end_device()) {
    client_state->is_low_end_device = spec.is_low_end_device();
  }
  if (spec.has_session_consistency_country()) {
    client_state->session_consistency_country =
        spec.session_consistency_country();
  }
  if (spec.has_permanent_consistency_country()) {
    client_state->permanent_consistency_country =
        spec.permanent_consistency_country();
  }
  if (spec.has_policy_restriction()) {
    switch (spec.policy_restriction()) {
      case CreateTrialsFromSeedTestCase_RestrictionPolicy_NO_RESTRICTIONS:
        client_state->policy_restriction = RestrictionPolicy::NO_RESTRICTIONS;
        break;
      case CreateTrialsFromSeedTestCase_RestrictionPolicy_CRITICAL_ONLY:
        client_state->policy_restriction = RestrictionPolicy::CRITICAL_ONLY;
        break;
      case CreateTrialsFromSeedTestCase_RestrictionPolicy_ALL:
        client_state->policy_restriction = RestrictionPolicy::ALL;
        break;
    }
  }
  return client_state;
}

void CreateTrialsFromSeedFuzzer(
    const variations::CreateTrialsFromSeedTestCase& test_case) {
  base::FieldTrialList field_trial_list;
  base::FeatureList feature_list;

  auto client_state = CreateClientFilterableState(
      test_case.has_client_filterable_state()
          ? test_case.client_filterable_state()
          : variations::CreateTrialsFromSeedTestCase::ClientFilterableState());
  auto entropy_providers = CreateEntropyProviders(
      test_case.has_entropy()
          ? test_case.entropy()
          : variations::CreateTrialsFromSeedTestCase::EntropyValues());

  if (!test_case.has_seed()) {
    return;
  }

  auto seed = test_case.seed();
  VariationsLayers layers(seed, entropy_providers);
  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, *client_state, base::BindRepeating(NoopUIStringOverrideCallback),
      entropy_providers, layers, &feature_list);
}

}  // namespace

DEFINE_PROTO_FUZZER(const variations::CreateTrialsFromSeedTestCase& test_case) {
  static Environment env;
  CreateTrialsFromSeedFuzzer(test_case);
}

}  // namespace variations
