// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/evaluate_seed.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "chromeos/crosapi/cpp/channel_to_enum.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/cros_evaluate_seed/cros_safe_seed_manager.h"
#include "components/variations/cros_evaluate_seed/cros_variations_field_trial_creator.h"
#include "components/variations/cros_evaluate_seed/early_boot_enabled_state_provider.h"
#include "components/variations/cros_evaluate_seed/early_boot_feature_visitor.h"
#include "components/variations/cros_evaluate_seed/early_boot_safe_seed.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_seed_store.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace variations::cros_early_boot::evaluate_seed {

namespace {

constexpr char kDefaultLocalStatePath[] = "/home/chronos/Local State";
constexpr char kDefaultUserDataDir[] = "/home/chronos/";

bool DetermineTrialState(std::unique_ptr<PrefService> local_state,
                         SafeSeed&& safe_seed,
                         featured::ComputedState* computed_state,
                         GetCrOSVariationsFieldTrialCreator get_creator) {
  // In the null seed case, featured just won't exec() evaluate_seed.
  SeedType seed_type =
      safe_seed.use_safe_seed ? SeedType::kSafeSeed : SeedType::kRegularSeed;
  CrOSSafeSeedManager safe_seed_manager(seed_type);

  std::optional<featured::SeedDetails> safe_seed_details;
  if (seed_type == SeedType::kSafeSeed) {
    safe_seed_details = std::move(safe_seed.seed_data);
  }

  CrosVariationsServiceClient client;

  std::unique_ptr<CrOSVariationsFieldTrialCreator> field_trial_creator =
      std::move(get_creator).Run(local_state.get(), &client, safe_seed_details);

  EarlyBootEnabledStateProvider enabled_state_provider;

  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(
          local_state.get(), &enabled_state_provider,
          /*backup_registry_key=*/std::wstring(),
          // Don't use a separate directory for safe mode prefs.
          /*user_data_dir=*/base::FilePath(),
          metrics::StartupVisibility::kForeground);
  metrics_state_manager->InstantiateFieldTrialList();

  auto feature_list = std::make_unique<base::FeatureList>();

  variations::VariationsIdsProvider::Create(
      variations::VariationsIdsProvider::Mode::kDontSendSignedInVariations);

  variations::PlatformFieldTrials platform_field_trials;
  variations::SyntheticTrialRegistry synthetic_trial_registry;

  // Despite documentation, SetUpFieldTrials returns false if it didn't use the
  // seed (e.g. if it used fieldtrial_testing_config), *not* just on failures.
  bool used_seed = field_trial_creator->SetUpFieldTrials(
      // TODO(http://b/297251107): implement overrides via chrome://flags.
      /*variation_ids=*/std::vector<std::string>(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          variations::switches::kForceVariationIds),
      /*extra_overrides=*/
      std::vector<base::FeatureList::FeatureOverrideInfo>(),
      std::move(feature_list), metrics_state_manager.get(),
      &synthetic_trial_registry, &platform_field_trials, &safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/false);

  if (used_seed) {
    if (seed_type == SeedType::kRegularSeed) {
      // We use the safe seed manager here because the
      // CrOSVariationsFieldTrialCreator (in the parent class's
      // CreateTrialsFromSeed) calls CrOSSafeSeedManager::SetActiveSeedState
      // when it marks a seed as active (NOT when it marks a seed as safe). This
      // is just to retrieve that active state, and doesn't necessarily indicate
      // that the seed is safe yet (we wait for ash to start to determine that).
      std::optional<featured::SeedDetails> details =
          safe_seed_manager.GetUsedSeed();
      if (details.has_value()) {
        computed_state->mutable_used_seed()->CopyFrom(details.value());
      } else {
        LOG(ERROR) << "Couldn't retrieve seed details; proceeding without them";
      }
    } else {
      // In this case, CrOSSafeSeedManager::SetActiveSeedState is never called,
      // so use the seed we requested to be used.
      CHECK_EQ(seed_type, SeedType::kSafeSeed);
      // Set above, at start of function.
      CHECK(safe_seed_details.has_value());
      computed_state->mutable_used_seed()->CopyFrom(safe_seed_details.value());
    }
  }

  // TODO(b/297870545): serialize correctly.
  // ideally, we want to add:
  // * (Early-boot?) trials that do not have features associated (e.g. for
  //   uniformity trials)
  // We also do not want to mark the trial as active yet, but given that
  // evaluate_seed will not report anything to UMA that isn't urgent.
  EarlyBootFeatureVisitor feature_visitor;
  base::FeatureList::VisitFeaturesAndParams(feature_visitor);
  google::protobuf::RepeatedPtrField<featured::FeatureOverride> overrides =
      feature_visitor.release_overrides();
  computed_state->mutable_overrides()->Assign(overrides.begin(),
                                              overrides.end());

  return true;
}
}  // namespace

// CreateLocalState creates an instance of a PrefService based on the json
// contents of |local_state_path|.
std::unique_ptr<PrefService> CreateLocalState(
    const base::FilePath& local_state_path) {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();

  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  ::variations::VariationsService::RegisterPrefs(pref_registry.get());

  int error_code = 0;
  std::string error_msg;
  JSONFileValueDeserializer deserializer(local_state_path);
  std::unique_ptr<base::Value> all_prefs_val =
      deserializer.Deserialize(&error_code, &error_msg);
  if (all_prefs_val == nullptr) {
    LOG(ERROR) << "Error deserializing local state: (code " << error_code
               << ") " << error_msg;
    return nullptr;
  }

  if (!all_prefs_val->is_dict()) {
    LOG(ERROR) << "Unexpected type: want dict, got " << all_prefs_val->type();
    return nullptr;
  }
  base::Value::Dict prefs_dict = std::move(*all_prefs_val).TakeDict();

  PrefServiceFactory pref_service_factory;
  auto local_state_pref_store = base::MakeRefCounted<InMemoryPrefStore>();

  for (auto [key, val] : prefs_dict) {
    local_state_pref_store->SetValue(
        key, std::move(val), WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }

  pref_service_factory.set_user_prefs(local_state_pref_store);

  return pref_service_factory.Create(pref_registry);
}

base::Version CrosVariationsServiceClient::GetVersionForSimulation() {
  // TODO(mutexlox): Get the version that will be used on restart instead of
  // the current version IF this is necessary. (We may not need simulations for
  // early-boot experiments.)
  // See ChromeVariationsServiceClient::GetVersionForSimulation.
  return version_info::GetVersion();
}

scoped_refptr<network::SharedURLLoaderFactory>
CrosVariationsServiceClient::GetURLLoaderFactory() {
  // Do not load any data on CrOS early boot. This function is only called to
  // fetch a new seed, and we should not fetch new seeds in evaluate_seed.
  return nullptr;
}

network_time::NetworkTimeTracker*
CrosVariationsServiceClient::GetNetworkTimeTracker() {
  // Do not load any data on CrOS early boot; evaluate_seed should not load new
  // seeds.
  return nullptr;
}

bool CrosVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
  // TODO(b/263975722): Implement.
  return false;
}

base::FilePath CrosVariationsServiceClient::GetVariationsSeedFileDir() {
  return base::FilePath(kDefaultUserDataDir);
}

bool CrosVariationsServiceClient::IsEnterprise() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnterpriseEnrolledSwitch);
}

// Get the active channel, if applicable.
version_info::Channel CrosVariationsServiceClient::GetChannel() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string channel;
  if (base::SysInfo::GetLsbReleaseValue(crosapi::kChromeOSReleaseTrack,
                                        &channel)) {
    return crosapi::ChannelToEnum(channel);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return version_info::Channel::UNKNOWN;
}

std::optional<SafeSeed> GetSafeSeedData(FILE* stream) {
  featured::SeedDetails safe_seed;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSafeSeedSwitch)) {
    // Read safe seed from |stream|.
    std::string safe_seed_data;
    if (!base::ReadStreamToString(stream, &safe_seed_data)) {
      PLOG(ERROR) << "Failed to read from stream:";
      return std::nullopt;
    }
    // Parse safe seed.
    if (!safe_seed.ParseFromString(safe_seed_data)) {
      LOG(ERROR) << "Failed to parse proto from input";
      return std::nullopt;
    }
    return SafeSeed{true, std::move(safe_seed)};
  }
  return SafeSeed{false, std::move(safe_seed)};
}

std::unique_ptr<CrOSVariationsFieldTrialCreator> GetFieldTrialCreator(
    PrefService* local_state,
    CrosVariationsServiceClient* client,
    const std::optional<featured::SeedDetails>& safe_seed_details) {
  std::unique_ptr<VariationsSafeSeedStore> safe_seed;
  if (safe_seed_details.has_value()) {
    safe_seed = std::make_unique<EarlyBootSafeSeed>(safe_seed_details.value());
  } else {
    safe_seed =
        std::make_unique<VariationsSafeSeedStoreLocalState>(local_state);
  }
  auto seed_store =
      std::make_unique<VariationsSeedStore>(local_state, std::move(safe_seed));

  return std::make_unique<CrOSVariationsFieldTrialCreator>(
      client, std::move(seed_store));
}

int EvaluateSeedMain(FILE* in_stream, FILE* out_stream) {
  return EvaluateSeedMain(in_stream, out_stream,
                          base::BindOnce(&GetFieldTrialCreator));
}

int EvaluateSeedMain(FILE* in_stream,
                     FILE* out_stream,
                     GetCrOSVariationsFieldTrialCreator get_creator) {
  std::optional<SafeSeed> safe_seed = GetSafeSeedData(in_stream);
  if (!safe_seed.has_value()) {
    LOG(ERROR) << "Failed to read seed from stdin";
    return EXIT_FAILURE;
  }

  // TODO(b/303882431): Set this up properly without races.
  base::FilePath local_state_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kLocalStatePathSwitch);
  if (local_state_path.empty()) {
    local_state_path = base::FilePath(kDefaultLocalStatePath);
  }

  std::unique_ptr<PrefService> local_state = CreateLocalState(local_state_path);
  if (!local_state) {
    LOG(ERROR) << "Failed to create local_state";
    return EXIT_FAILURE;
  }

  featured::ComputedState computed_state;
  if (!DetermineTrialState(std::move(local_state), std::move(safe_seed.value()),
                           &computed_state, std::move(get_creator))) {
    LOG(ERROR) << "Failed to determine trial state; will use defaults";
    return EXIT_FAILURE;
  }

  base::File out = base::FILEToFile(out_stream);
  if (!out.IsValid()) {
    LOG(ERROR) << "Failed to open output";
    return EXIT_FAILURE;
  }

  std::string out_str;
  if (!computed_state.SerializeToString(&out_str)) {
    LOG(ERROR) << "Failed to serialize state";
    return EXIT_FAILURE;
  }

  if (!out.WriteAtCurrentPosAndCheck(base::as_byte_span(out_str))) {
    LOG(ERROR) << "Failed to write to output";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

}  // namespace variations::cros_early_boot::evaluate_seed
