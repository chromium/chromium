// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator_base.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstdint>
#include <memory>
#include <set>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/limited_entropy_mode_gate.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/service/limited_entropy_randomization.h"
#include "components/variations/service/limited_entropy_synthetic_trial.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/service/variations_service_utils.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "ui/base/device_form_factor.h"

namespace variations {
namespace {

// Records the loaded seed's expiry status.
void RecordSeedExpiry(bool is_safe_seed, VariationsSeedExpiry seed_expiry) {
  const std::string histogram_name =
      is_safe_seed ? "Variations.SafeMode.CreateTrials.SeedExpiry"
                   : "Variations.CreateTrials.SeedExpiry";
  base::UmaHistogramEnumeration(histogram_name, seed_expiry);
}

// Records the loaded seed's age.
void RecordSeedFreshness(base::TimeDelta seed_age) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.SeedFreshness", seed_age.InMinutes(),
                              1, base::Days(30).InMinutes(), 50);
}

// Records details about Chrome's attempt to apply a variations seed.
void RecordVariationsSeedUsage(SeedUsage usage) {
  VLOG(1) << "VariationsSeedUsage:" << static_cast<int>(usage);
  base::UmaHistogramEnumeration("Variations.SeedUsage", usage);
}

// If an invalid command-line to force field trials was specified, exit the
// browser with a helpful error message, so that the user can correct their
// mistake.
void ExitWithMessage(const std::string& message) {
  puts(message.c_str());
  exit(1);
}

// Retrieves the value of the policy converted to the RestrictionPolicyValues.
RestrictionPolicy GetVariationPolicyRestriction(PrefService* local_state) {
  int value = local_state->GetInteger(prefs::kVariationsRestrictionsByPolicy);

  // If the value form the pref is invalid use the default value.
  if (value < 0 || value > static_cast<int>(RestrictionPolicy::kMaxValue)) {
    return RestrictionPolicy::NO_RESTRICTIONS;
  }

  return static_cast<RestrictionPolicy>(value);
}

Study::CpuArchitecture GetCurrentCpuArchitecture() {
  std::string process_arch = base::SysInfo::ProcessCPUArchitecture();
  if (process_arch == "ARM_64") {
    return Study::ARM64;
  }
  if (process_arch == "ARM") {
    return Study::ARM32;
  }
  if (process_arch == "x86") {
    return Study::X86_32;
  }
  if (process_arch == "x86_64") {
    std::string os_arch = base::SysInfo::OperatingSystemArchitecture();
    if (base::StartsWith(os_arch, "arm",
                         base::CompareCase::INSENSITIVE_ASCII) ||
        base::EqualsCaseInsensitiveASCII(os_arch, "aarch64")) {
      // x86-64 binary running on an arm64 host via the Rosetta 2 binary
      // translator.
      return Study::TRANSLATED_X86_64;
    }
    return Study::X86_64;
  }
  NOTREACHED_IN_MIGRATION();
  return Study::X86_64;
}

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
// Determines whether the field trial testing config defined in
// testing/variations/fieldtrial_testing_config.json should be applied. If the
// "disable_fieldtrial_testing_config" GN flag is set to true, then the testing
// config should never be applied. Otherwise, if the build is a Chrome-branded
// build, then the testing config should only be applied if either the
// "--enable-field-trial-config" or
// "--enable-benchmarking=enable-field-trial-config" switch is passed. For
// non-Chrome branded builds, by default, the testing config is applied, unless
// the "--disable-field-trial-config" and/or "--variations-server-url" switches
// are passed and no enabling switches are set.
bool ShouldUseFieldTrialTestingConfig(const base::CommandLine* command_line) {
  bool is_enable_switch_set =
      command_line->HasSwitch(switches::kEnableFieldTrialTestingConfig) ||
      command_line->GetSwitchValueASCII(
          variations::switches::kEnableBenchmarking) ==
          switches::kEnableFieldTrialTestingConfig;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return is_enable_switch_set;
#else
  return is_enable_switch_set ||
         (!command_line->HasSwitch(switches::kDisableFieldTrialTestingConfig) &&
          !command_line->HasSwitch(switches::kVariationsServerURL));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

// Causes Chrome to start watching for browser crashes if the following
// conditions are met:
// 1. This is not a background session.
// 2. Extended Variations Safe Mode is supported on this platform.
void MaybeExtendVariationsSafeMode(
    metrics::MetricsStateManager* metrics_state_manager) {
  if (metrics_state_manager->is_background_session()) {
    // If the session is expected to be a background session, then do not start
    // watching for browser crashes here. This monitoring is not desired in
    // background sessions, whose terminations should never be considered
    // crashes.
    return;
  }
  if (!metrics_state_manager->IsExtendedSafeModeSupported()) {
    return;
  }

  metrics_state_manager->LogHasSessionShutdownCleanly(
      /*has_session_shutdown_cleanly=*/false,
      /*is_extended_safe_mode=*/true);
}

// Returns true iff the given seed contains a layer with LIMITED entropy mode.
bool ContainsLimitedEntropyLayer(const VariationsSeed& seed) {
  for (const Layer& layer_proto : seed.layers()) {
    if (layer_proto.entropy_mode() == Layer::LIMITED) {
      return true;
    }
  }
  return false;
}

}  // namespace

BASE_FEATURE(kForceFieldTrialSetupCrashForTesting,
             "ForceFieldTrialSetupCrashForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

Study::Channel ConvertProductChannelToStudyChannel(
    version_info::Channel product_channel) {
  switch (product_channel) {
    case version_info::Channel::CANARY:
      return Study::CANARY;
    case version_info::Channel::DEV:
      return Study::DEV;
    case version_info::Channel::BETA:
      return Study::BETA;
    case version_info::Channel::STABLE:
      return Study::STABLE;
    case version_info::Channel::UNKNOWN:
      return Study::UNKNOWN;
  }
  NOTREACHED_IN_MIGRATION();
  return Study::UNKNOWN;
}

VariationsFieldTrialCreatorBase::VariationsFieldTrialCreatorBase(
    VariationsServiceClient* client,
    std::unique_ptr<VariationsSeedStore> seed_store,
    base::OnceCallback<std::string(PrefService*)> locale_cb,
    LimitedEntropySyntheticTrial* limited_entropy_synthetic_trial)
    : client_(client),
      seed_store_(std::move(seed_store)),
      create_trials_from_seed_called_(false),
      application_locale_(std::move(locale_cb).Run(seed_store_->local_state())),
      has_platform_override_(false),
      platform_override_(Study::PLATFORM_WINDOWS),
      limited_entropy_synthetic_trial_(limited_entropy_synthetic_trial) {}

VariationsFieldTrialCreatorBase::~VariationsFieldTrialCreatorBase() = default;

std::string VariationsFieldTrialCreatorBase::GetLatestCountry() const {
  const std::string override_country = base::ToLowerASCII(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVariationsOverrideCountry));
  return !override_country.empty()
             ? override_country
             : local_state()->GetString(prefs::kVariationsCountry);
}

bool VariationsFieldTrialCreatorBase::SetUpFieldTrials(
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids,
    const std::vector<base::FeatureList::FeatureOverrideInfo>& extra_overrides,
    std::unique_ptr<base::FeatureList> feature_list,
    metrics::MetricsStateManager* metrics_state_manager,
    SyntheticTrialRegistry* synthetic_trial_registry,
    PlatformFieldTrials* platform_field_trials,
    SafeSeedManagerBase* safe_seed_manager,
    bool add_entropy_source_to_variations_ids) {
  DCHECK(feature_list);
  DCHECK(metrics_state_manager);
  DCHECK(platform_field_trials);
  DCHECK(safe_seed_manager);
  CHECK(client_);

  MaybeExtendVariationsSafeMode(metrics_state_manager);

  // TODO(crbug.com/40796250): Some FieldTrial-setup-related code is here and
  // some is in MetricsStateManager::InstantiateFieldTrialList(). It's not ideal
  // that it's in two places.
  VariationsIdsProvider* http_header_provider =
      VariationsIdsProvider::GetInstance();

  if (add_entropy_source_to_variations_ids) {
    http_header_provider->SetLowEntropySourceValue(
        metrics_state_manager->GetLowEntropySource());
  }
  // Force the variation ids selected in chrome://flags and/or specified using
  // the command-line flag.
  auto result = http_header_provider->ForceVariationIds(
      variation_ids, command_line_variation_ids);

  switch (result) {
    case VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY:
      ExitWithMessage(base::StringPrintf("Invalid --%s list specified.",
                                         switches::kForceVariationIds));
      break;
    case VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY:
      // It should not be possible to have invalid variation ids from the
      // vector param (which corresponds to chrome://flags).
      NOTREACHED_IN_MIGRATION();
      break;
    case VariationsIdsProvider::ForceIdsResult::SUCCESS:
      break;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool success = http_header_provider->ForceDisableVariationIds(
      command_line->GetSwitchValueASCII(switches::kForceDisableVariationIds));
  if (!success) {
    ExitWithMessage(base::StringPrintf("Invalid --%s list specified.",
                                       switches::kForceDisableVariationIds));
  }

  feature_list->InitFromCommandLine(
      command_line->GetSwitchValueASCII(::switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(::switches::kDisableFeatures));

  // This needs to happen here: After the InitFromCommandLine() call,
  // because the explicit cmdline --disable-features and --enable-features
  // should take precedence over these extra overrides. Before the call to
  // SetInstance(), because overrides cannot be registered after the FeatureList
  // instance is set.
  feature_list->RegisterExtraFeatureOverrides(extra_overrides);

  bool used_testing_config = false;
  // TODO(crbug.com/40230862): Remove this code path.
#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
  if (ShouldUseFieldTrialTestingConfig(command_line)) {
    ApplyFieldTrialTestingConfig(feature_list.get());
    used_testing_config = true;
  }
#else
  if (command_line->HasSwitch(switches::kEnableFieldTrialTestingConfig)) {
    ExitWithMessage(
        base::StringPrintf("--%s was passed, but the field trial testing "
                           "config was excluded from the build.",
                           switches::kEnableFieldTrialTestingConfig));
  }
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
  if (command_line->HasSwitch(switches::kVariationsTestSeedJsonPath)) {
    LoadSeedFromJsonFile(
        command_line->GetSwitchValuePath(switches::kVariationsTestSeedJsonPath));
  }

  auto entropy_providers = metrics_state_manager->CreateEntropyProviders(
      IsLimitedEntropyRandomizationSourceEnabled(
          client_->GetChannelForVariations(),
          limited_entropy_synthetic_trial_));

  bool used_seed = false;
  if (!used_testing_config) {
    used_seed =
        CreateTrialsFromSeed(*entropy_providers, feature_list.get(),
                             safe_seed_manager, synthetic_trial_registry);
  }

  platform_field_trials->SetUpClientSideFieldTrials(
      used_seed, *entropy_providers, feature_list.get());

  platform_field_trials->RegisterFeatureOverrides(feature_list.get());

  base::FeatureList::SetInstance(std::move(feature_list));

  // For testing Variations Safe Mode, maybe crash here.
  if (base::FeatureList::IsEnabled(kForceFieldTrialSetupCrashForTesting)) {
    // Terminate with a custom exit test code. See
    // VariationsSafeModeEndToEndBrowserTest.ExtendedSafeSeedEndToEnd.
    base::Process::TerminateCurrentProcessImmediately(0x7E57C0D3);
  }

  // This must be called after |local_state_| is initialized.
  platform_field_trials->OnVariationsSetupComplete();

  VLOG(1) << "VariationsSetupComplete";

  return used_seed;
}

std::unique_ptr<ClientFilterableState>
VariationsFieldTrialCreatorBase::GetClientFilterableStateForVersion(
    const base::Version& version) {
  // Note that passing base::Unretained(client_) is safe here because |client_|
  // lives until Chrome exits.
  auto IsEnterpriseCallback = base::BindRepeating(
      &VariationsServiceClient::IsEnterprise, base::Unretained(client_));
  auto GoogleGroupsCallback = base::BindRepeating(
      &VariationsFieldTrialCreatorBase::GetGoogleGroupsFromPrefs,
      base::Unretained(this));
  std::unique_ptr<ClientFilterableState> state =
      std::make_unique<ClientFilterableState>(IsEnterpriseCallback,
                                              GoogleGroupsCallback);
  state->locale = application_locale_;
  state->reference_date = GetSeedStore()->GetTimeForStudyDateChecks(
      /*is_safe_seed=*/false);
  state->version = version;
  state->os_version = ClientFilterableState::GetOSVersion();
  state->channel =
      ConvertProductChannelToStudyChannel(client_->GetChannelForVariations());
  state->form_factor = GetCurrentFormFactor();
  state->cpu_architecture = GetCurrentCpuArchitecture();
  state->platform = GetPlatform();
  state->hardware_class = ClientFilterableState::GetHardwareClass();
#if BUILDFLAG(IS_ANDROID)
  // This is set on Android only currently, because the IsLowEndDevice() API
  // on other platforms has no intrinsic meaning outside of a field trial that
  // controls its value. Since this is before server-side field trials are
  // evaluated, that field trial would not be able to apply for this case.
  state->is_low_end_device = base::SysInfo::IsLowEndDevice();
#endif
  state->session_consistency_country = GetLatestCountry();
  state->permanent_consistency_country = LoadPermanentConsistencyCountry(
      version, state->session_consistency_country);
  state->policy_restriction = GetVariationPolicyRestriction(local_state());
  return state;
}

std::string VariationsFieldTrialCreatorBase::LoadPermanentConsistencyCountry(
    const base::Version& version,
    const std::string& latest_country) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(version.IsValid());

  const std::string override_country = base::ToLowerASCII(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVariationsOverrideCountry));
  if (!override_country.empty()) {
    return override_country;
  }

  const std::string permanent_overridden_country =
      local_state()->GetString(prefs::kVariationsPermanentOverriddenCountry);

  if (!permanent_overridden_country.empty()) {
    base::UmaHistogramEnumeration(
        "Variations.LoadPermanentConsistencyCountryResult",
        LOAD_COUNTRY_HAS_PERMANENT_OVERRIDDEN_COUNTRY, LOAD_COUNTRY_MAX);
    return permanent_overridden_country;
  }

  const base::Value::List& list_value =
      local_state()->GetList(prefs::kVariationsPermanentConsistencyCountry);
  const std::string* stored_version_string = nullptr;
  const std::string* stored_country = nullptr;

  // Determine if the saved pref value is present and valid.
  const bool is_pref_empty = list_value.empty();
  const bool is_pref_valid =
      list_value.size() == 2 &&
      (stored_version_string = list_value[0].GetIfString()) &&
      (stored_country = list_value[1].GetIfString()) &&
      base::Version(*stored_version_string).IsValid();

  // Determine if the version from the saved pref matches |version|.
  const bool does_version_match =
      is_pref_valid && version == base::Version(*stored_version_string);

  // Determine if the country in the saved pref matches the country in
  // |latest_country|.
  const bool does_country_match = is_pref_valid && !latest_country.empty() &&
                                  *stored_country == latest_country;

  // Record a histogram for how the saved pref value compares to the current
  // version and the country code in the variations seed.
  LoadPermanentConsistencyCountryResult result;
  if (is_pref_empty) {
    result = !latest_country.empty() ? LOAD_COUNTRY_NO_PREF_HAS_SEED
                                     : LOAD_COUNTRY_NO_PREF_NO_SEED;
  } else if (!is_pref_valid) {
    result = !latest_country.empty() ? LOAD_COUNTRY_INVALID_PREF_HAS_SEED
                                     : LOAD_COUNTRY_INVALID_PREF_NO_SEED;
  } else if (latest_country.empty()) {
    result = does_version_match ? LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_EQ
                                : LOAD_COUNTRY_HAS_PREF_NO_SEED_VERSION_NEQ;
  } else if (does_version_match) {
    result = does_country_match ? LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_EQ
                                : LOAD_COUNTRY_HAS_BOTH_VERSION_EQ_COUNTRY_NEQ;
  } else {
    result = does_country_match ? LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_EQ
                                : LOAD_COUNTRY_HAS_BOTH_VERSION_NEQ_COUNTRY_NEQ;
  }
  UMA_HISTOGRAM_ENUMERATION("Variations.LoadPermanentConsistencyCountryResult",
                            result, LOAD_COUNTRY_MAX);

  // Use the stored country if one is available and was fetched since the last
  // time Chrome was updated.
  if (does_version_match) {
    return *stored_country;
  }

  if (latest_country.empty()) {
    if (!is_pref_valid) {
      local_state()->ClearPref(prefs::kVariationsPermanentConsistencyCountry);
    }
    // If we've never received a country code from the server, use an empty
    // country so that it won't pass any filters that specifically include
    // countries, but so that it will pass any filters that specifically exclude
    // countries.
    return std::string();
  }

  // Otherwise, update the pref with the current Chrome version and country.
  StorePermanentCountry(version, latest_country);
  return latest_country;
}

void VariationsFieldTrialCreatorBase::StorePermanentCountry(
    const base::Version& version,
    const std::string& country) {
  base::Value::List new_list_value;
  new_list_value.Append(version.GetString());
  new_list_value.Append(country);
  local_state()->SetList(prefs::kVariationsPermanentConsistencyCountry,
                         std::move(new_list_value));
}

void VariationsFieldTrialCreatorBase::StoreVariationsOverriddenCountry(
    const std::string& country) {
  local_state()->SetString(prefs::kVariationsPermanentOverriddenCountry,
                           country);
}

void VariationsFieldTrialCreatorBase::OverrideVariationsPlatform(
    Study::Platform platform_override) {
  has_platform_override_ = true;
  platform_override_ = platform_override;
}

Study::Platform VariationsFieldTrialCreatorBase::GetPlatform() {
  if (has_platform_override_) {
    return platform_override_;
  }
  return ClientFilterableState::GetCurrentPlatform();
}

Study::FormFactor VariationsFieldTrialCreatorBase::GetCurrentFormFactor() {
  return client_->GetCurrentFormFactor();
}

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
void VariationsFieldTrialCreatorBase::ApplyFieldTrialTestingConfig(
    base::FeatureList* feature_list) {
  VLOG(1) << "Applying FieldTrialTestingConfig";
  // Note that passing base::Unretained(this) below is safe because the callback
  // is executed synchronously.
  AssociateDefaultFieldTrialConfig(
      base::BindRepeating(&VariationsFieldTrialCreatorBase::OverrideUIString,
                          base::Unretained(this)),
      GetPlatform(), GetCurrentFormFactor(), feature_list);
}
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

base::Time VariationsFieldTrialCreatorBase::CalculateSeedFreshness() {
  // TODO(crbug.com/40274989): Consider comparing the server-provided fetch time
  // with the network time.
  return seed_type_ == SeedType::kSafeSeed
             ? GetSeedStore()->GetSafeSeedFetchTime()
             : GetSeedStore()->GetLastFetchTime();
}

bool VariationsFieldTrialCreatorBase::HasSeedExpired() {
  const base::Time fetch_time = CalculateSeedFreshness();
  // If the fetch time is null, skip the expiry check. If the seed is a regular
  // seed (i.e. not a safe seed) and the fetch time is missing, then this must
  // be the first run of Chrome. If the seed is a safe seed, the fetch time may
  // be missing because the pref was added about a milestone later than most of
  // the other safe seed prefs.
  if (fetch_time.is_null()) {
    RecordSeedExpiry(seed_type_ == SeedType::kSafeSeed,
                     VariationsSeedExpiry::kFetchTimeMissing);
    if (seed_type_ != SeedType::kSafeSeed) {
      // Store the current time as the last fetch time for Chrome's first run.
      GetSeedStore()->RecordLastFetchTime(base::Time::Now());
      // Record freshness of "0", since we expect a first run seed to be fresh.
      RecordSeedFreshness(base::TimeDelta());
    }
    return false;
  }
  bool has_seed_expired = HasSeedExpiredSinceTime(fetch_time);
  if (!has_seed_expired) {
    RecordSeedFreshness(base::Time::Now() - fetch_time);
  }
  RecordSeedExpiry(seed_type_ == SeedType::kSafeSeed,
                   has_seed_expired ? VariationsSeedExpiry::kExpired
                                    : VariationsSeedExpiry::kNotExpired);
  return has_seed_expired;
}

bool VariationsFieldTrialCreatorBase::IsSeedForFutureMilestone(
    bool is_safe_seed) {
  int seed_milestone = is_safe_seed ? GetSeedStore()->GetSafeSeedMilestone()
                                    : GetSeedStore()->GetLatestMilestone();

  // The regular and safe seed milestone prefs were added in M97, so the prefs
  // are not populated for seeds stored before then.
  if (!seed_milestone) {
    return false;
  }

  int client_milestone = version_info::GetMajorVersionNumberAsInt();
  return seed_milestone > client_milestone;
}

base::flat_set<uint64_t>
VariationsFieldTrialCreatorBase::GetGoogleGroupsFromPrefs() {
  // Before using Google groups information, ensure that there any information
  // for already-deleted profiles has been removed.
  //
  // TODO(b/264838828): move this call to be done in SetUpFieldTrials().  The
  // reason it is currently done here is simply to allow a safer gradual
  // rollout of the initial feature, as this code is only run if there is at
  // least one study that filters by Google group membership.
  client_->RemoveGoogleGroupsFromPrefsForDeletedProfiles(local_state());

  base::flat_set<uint64_t> groups = base::flat_set<uint64_t>();

  const base::Value::Dict& profiles_dict =
      local_state()->GetDict(prefs::kVariationsGoogleGroups);
  for (const auto profile : profiles_dict) {
    const base::Value::List& profile_groups = profile.second.GetList();
    for (const auto& group_value : profile_groups) {
      const std::string* group = group_value.GetIfString();
      if (!group || group->empty()) {
        continue;
      }
      uint64_t group_id;
      if (!base::StringToUint64(*group, &group_id)) {
        continue;
      }
      groups.insert(group_id);
    }
  }
  return groups;
}

bool VariationsFieldTrialCreatorBase::
    ShouldActivateLimitedEntropySyntheticTrial(const VariationsSeed& seed) {
  return limited_entropy_synthetic_trial_ &&
         IsLimitedEntropyModeEnabled(client_->GetChannelForVariations()) &&
         ContainsLimitedEntropyLayer(seed);
}

void VariationsFieldTrialCreatorBase::
    RegisterLimitedEntropySyntheticTrialIfNeeded(
        const VariationsSeed& seed,
        SyntheticTrialRegistry* synthetic_trial_registry) {
  if (ShouldActivateLimitedEntropySyntheticTrial(seed)) {
    limited_entropy_synthetic_trial_->Register(*synthetic_trial_registry);
  }
}

bool VariationsFieldTrialCreatorBase::CreateTrialsFromSeed(
    const EntropyProviders& entropy_providers,
    base::FeatureList* feature_list,
    SafeSeedManagerBase* safe_seed_manager,
    SyntheticTrialRegistry* synthetic_trial_registry) {
  // This histogram name uses "VariationsFieldTrialCreator" rather than
  // "VariationsFieldTrialCreatorBase" for consistency with historical data
  TRACE_EVENT0("startup", "VariationsFieldTrialCreator::CreateTrialsFromSeed");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!create_trials_from_seed_called_);
  CHECK(client_);
  create_trials_from_seed_called_ = true;

  base::TimeTicks start_time = base::TimeTicks::Now();

  const base::Version& current_version = version_info::GetVersion();
  if (!current_version.IsValid()) {
    return false;
  }

  std::unique_ptr<ClientFilterableState> client_filterable_state =
      GetClientFilterableStateForVersion(current_version);
  base::UmaHistogramSparse("Variations.UserChannel",
                           client_filterable_state->channel);
  base::UmaHistogramEnumeration("Variations.PolicyRestriction",
                                client_filterable_state->policy_restriction);

  seed_type_ = safe_seed_manager->GetSeedType();
  // If we have tried safe seed and we still get crashes, try null seed.
  if (seed_type_ == SeedType::kNullSeed) {
    RecordVariationsSeedUsage(SeedUsage::kNullSeedUsed);
    return false;
  }

  VariationsSeed seed;

  std::string seed_data;              // Only set if not in safe mode.
  std::string base64_seed_signature;  // Only set if not in safe mode.
  const bool run_in_safe_mode = seed_type_ == SeedType::kSafeSeed;
  const bool seed_loaded =
      run_in_safe_mode
          ? GetSeedStore()->LoadSafeSeed(&seed, client_filterable_state.get())
          : GetSeedStore()->LoadSeed(&seed, &seed_data, &base64_seed_signature);
  if (!seed_loaded) {
    // If Chrome should run in safe mode but the safe seed was not successfully
    // loaded, then do not apply a seed. Fall back to client-side defaults.
    RecordVariationsSeedUsage(run_in_safe_mode
                                  ? SeedUsage::kUnloadableSafeSeedNotUsed
                                  : SeedUsage::kUnloadableRegularSeedNotUsed);
    return false;
  }
  if (HasSeedExpired()) {
    RecordVariationsSeedUsage(run_in_safe_mode
                                  ? SeedUsage::kExpiredSafeSeedNotUsed
                                  : SeedUsage::kExpiredRegularSeedNotUsed);
    return false;
  }
  if (IsSeedForFutureMilestone(/*is_safe_seed=*/run_in_safe_mode)) {
    RecordVariationsSeedUsage(
        run_in_safe_mode ? SeedUsage::kSafeSeedForFutureMilestoneNotUsed
                         : SeedUsage::kRegularSeedForFutureMilestoneNotUsed);
    return false;
  }
  RecordVariationsSeedUsage(run_in_safe_mode ? SeedUsage::kSafeSeedUsed
                                             : SeedUsage::kRegularSeedUsed);
  SetSeedVersion(seed.version());

  RegisterLimitedEntropySyntheticTrialIfNeeded(seed, synthetic_trial_registry);
  VariationsLayers layers(seed, entropy_providers);

  // The server is not expected to send a seed with misconfigured entropy. Just
  // in case there is an unexpected server-side bug and the entropy is
  // misconfigured, return early to skip assigning any trials from the seed.
  // Also, generate a crash report, so that the misconfigured seed can be
  // identified and rolled back.
  //
  // Checking `IsLimitedEntropyModeEnabled()` is a safety measure, but is
  // redundant given that `VariationsLayers` ensures that no layer with
  // `EntropyMode.LIMITED` is marked as active for clients without a limited
  // entropy provider (i.e. have limited entropy mode disabled, see
  // `IsLimitedEntropyRandomizationSourceEnabled()`). For such clients,
  // `SeedHasMisconfiguredEntropy()` will always be false.
  if (IsLimitedEntropyModeEnabled(client_->GetChannelForVariations()) &&
      SeedHasMisconfiguredEntropy(layers, seed)) {
    base::debug::DumpWithoutCrashing();
    return false;
  }

  // Note that passing base::Unretained(this) below is safe because the callback
  // is executed synchronously. It is not possible to pass UIStringOverrider
  // directly to VariationsSeedProcessor (which is in components/variations and
  // not components/variations/service) as the variations component should not
  // depend on //ui/base.
  VariationsSeedProcessor().CreateTrialsFromSeed(
      seed, *client_filterable_state,
      base::BindRepeating(&VariationsFieldTrialCreatorBase::OverrideUIString,
                          base::Unretained(this)),
      entropy_providers, layers, feature_list);

  VLOG(1) << "CreateTrialsFromSeed complete with "
          << "seed.version='" << seed.version() << "'";

  // Store into the |safe_seed_manager| the combined server and client data used
  // to create the field trials. But, as an optimization, skip this step when
  // running in safe mode – once running in safe mode, there can never be a need
  // to save the active state to the safe seed prefs.
  if (!run_in_safe_mode) {
    safe_seed_manager->SetActiveSeedState(
        seed_data, base64_seed_signature,
        local_state()->GetInteger(prefs::kVariationsSeedMilestone),
        std::move(client_filterable_state), seed_store_->GetLastFetchTime());
  }

  base::UmaHistogramCounts1M("Variations.AppliedSeed.Size", seed_data.size());
  base::UmaHistogramTimes("Variations.SeedProcessingTime",
                          base::TimeTicks::Now() - start_time);
  return true;
}

void VariationsFieldTrialCreatorBase::LoadSeedFromJsonFile(
    const base::FilePath& json_seed_path) {
  VLOG(1) << "Loading seed from JSON file:" << json_seed_path;
  JSONFileValueDeserializer file_deserializer(json_seed_path);
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> json_contents =
      file_deserializer.Deserialize(&error_code, &error_message);

  if (!json_contents) {
    ExitWithMessage(base::StringPrintf("Failed to load \"%s\" %s (%i)",
                                       json_seed_path.AsUTF8Unsafe().c_str(),
                                       error_message.c_str(), error_code));
  }

  const base::Value* seed_data =
      json_contents->GetDict().Find(prefs::kVariationsCompressedSeed);
  const base::Value* seed_signature =
      json_contents->GetDict().Find(prefs::kVariationsSeedSignature);

  if (!seed_data || !seed_data->is_string()) {
    ExitWithMessage(
        base::StringPrintf("Missing or invalid seed data in contents of \"%s\"",
                           json_seed_path.AsUTF8Unsafe().c_str()));
  }

  if (!seed_signature || !seed_signature->is_string()) {
    ExitWithMessage(base::StringPrintf(
        "Missing or invalid seed signature in contents of \"%s\"",
        json_seed_path.AsUTF8Unsafe().c_str()));
  }

  // Set fail counters to 0 to make sure Chrome doesn't run in variations safe
  // mode. This ensures that Chrome won't use the safe seed.
  local_state()->SetInteger(prefs::kVariationsCrashStreak, 0);
  local_state()->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  // Override Local State seed prefs.
  local_state()->SetString(prefs::kVariationsCompressedSeed,
                           seed_data->GetString());
  local_state()->SetString(prefs::kVariationsSeedSignature,
                           seed_signature->GetString());

  local_state()->CommitPendingWrite();  // Schedule a write to Local State.
}

VariationsSeedStore* VariationsFieldTrialCreatorBase::GetSeedStore() {
  return seed_store_.get();
}

// static
bool VariationsFieldTrialCreatorBase::
    IsLimitedEntropyRandomizationSourceEnabled(
        version_info::Channel channel,
        LimitedEntropySyntheticTrial* trial) {
  // Channel gated clients should not generate a limited entropy randomization
  // source.
  if (!IsLimitedEntropyModeEnabled(channel)) {
    return false;
  }
  // Only clients in the enabled group of the limited entropy synthetic trial
  // should have a limited entropy randomization source.
  return trial && trial->IsEnabled();
}

}  // namespace variations
