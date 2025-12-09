// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_field_trial_creator.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstdint>
#include <memory>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/language/core/browser/locale_util.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/platform_field_trials.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/service/buildflags.h"
#include "components/variations/service/limited_entropy_randomization.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/service/variations_service_utils.h"
#include "components/variations/variations_features.h"
#include "components/variations/variations_ids_provider.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

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
  base::UmaHistogramCustomCounts("Variations.SeedFreshness",
                                 seed_age.InMinutes(), 1,
                                 base::Days(30).InMinutes(), 50);
}

// Records details about Chrome's attempt to apply a variations seed.
void RecordVariationsSeedUsage(SeedUsage usage) {
  VLOG(1) << "VariationsSeedUsage:" << static_cast<int>(usage);
  base::UmaHistogramEnumeration("Variations.SeedUsage", usage);
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  NOTREACHED();
#else
  // Return a fake value for unsupported architectures
  // instead of using NOTREACHED() to cause a crash
  // on Chromium builds
  return Study::X86_64;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
  NOTREACHED();
}

void MaybeActivateMetricsNoopTrial() {
  if (base::FieldTrial* trial =
          base::FieldTrialList::Find("MetricsNoopRegressionAutoAdvance")) {
    // The original plan was to randomly activate the field trial half the time,
    // but the rand() function was not seeded resulting in none of the Enabled
    // group was activated. Nevertheles, this is an interesting edge case for
    // us to test so keep this around for now. The replacement is
    // MetricsNoopRegressionAutoAdvance2 below.
    if (trial->GetGroupNameWithoutActivation() == "Enabled") {
      if (rand() % 2 == 0) {
        trial->Activate();
      }
    } else {
      trial->Activate();
    }
  }
}

void MaybeActivateMetricsNoopTrial2() {
  if (base::FieldTrial* trial =
          base::FieldTrialList::Find("MetricsNoopRegressionAutoAdvance2")) {
    // If the user is in the Enabled group, we want to randomly activate the
    // field trial half the time.
    if (trial->GetGroupNameWithoutActivation() == "Enabled") {
      if (base::RandBool()) {
        trial->Activate();
      }
    } else {
      trial->Activate();
    }
  }
}

}  // namespace

BASE_FEATURE(kForceFieldTrialSetupCrashForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool CreateTrialsResult::AppliedSeedHasActiveLimitedLayer() const {
  if (!applied_seed) {
    return false;
  }
  return seed_has_active_limited_layer.value_or(false);
}

VariationsFieldTrialCreator::VariationsFieldTrialCreator(
    VariationsServiceClient* client,
    std::unique_ptr<VariationsSeedStore> seed_store,
    const UIStringOverrider& ui_string_overrider)
    : client_(client),
      seed_store_(std::move(seed_store)),
      application_locale_(
          language::GetApplicationLocale(seed_store_->local_state())),
      ui_string_overrider_(ui_string_overrider),
      sticky_activation_manager_(seed_store_->local_state(),
                                 client->IsStickyActivationEnabled()) {}

VariationsFieldTrialCreator::~VariationsFieldTrialCreator() = default;

std::string VariationsFieldTrialCreator::GetLatestCountry() const {
  const std::string override_country = base::ToLowerASCII(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVariationsOverrideCountry));
  return !override_country.empty()
             ? override_country
             : seed_store_->GetLatestCountry();
}

bool VariationsFieldTrialCreator::SetUpFieldTrials(
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids,
    const std::vector<base::FeatureList::FeatureOverrideInfo>& extra_overrides,
    std::unique_ptr<base::FeatureList> feature_list,
    metrics::MetricsStateManager* metrics_state_manager,
    PlatformFieldTrials* platform_field_trials,
    SafeSeedManager* safe_seed_manager,
    bool add_entropy_source_to_variations_ids,
    const EntropyProviders& entropy_providers) {
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

  // Force the variation ids selected in chrome://flags and/or specified using
  // the command-line flag.
  auto result = http_header_provider->ForceVariationIds(
      base::PassKey<VariationsFieldTrialCreator>(),
      variation_ids, command_line_variation_ids);

  switch (result) {
    case VariationsIdsProvider::ForceIdsResult::INVALID_SWITCH_ENTRY:
      client_->ExitWithMessage(base::StringPrintf(
          "Invalid --%s list specified.", switches::kForceVariationIds));
      break;
    case VariationsIdsProvider::ForceIdsResult::INVALID_VECTOR_ENTRY:
      // It should not be possible to have invalid variation ids from the
      // vector param (which corresponds to chrome://flags).
      NOTREACHED();
    case VariationsIdsProvider::ForceIdsResult::SUCCESS:
      break;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  bool success = http_header_provider->ForceDisableVariationIds(
      command_line->GetSwitchValueASCII(switches::kForceDisableVariationIds));
  if (!success) {
    client_->ExitWithMessage(base::StringPrintf("Invalid --%s list specified.",
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
    client_->ExitWithMessage(
        base::StringPrintf("--%s was passed, but the field trial testing "
                           "config was excluded from the build.",
                           switches::kEnableFieldTrialTestingConfig));
  }
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
  if (command_line->HasSwitch(switches::kVariationsTestSeedJsonPath)) {
    LoadSeedFromJsonFile(command_line->GetSwitchValuePath(
        switches::kVariationsTestSeedJsonPath));
  }

  // Get client filterable state to be used by CreateTrialsFromSeed()
  std::unique_ptr<ClientFilterableState> client_filterable_state = nullptr;
  const base::Version& current_version = version_info::GetVersion();
  if (current_version.IsValid()) {
    client_filterable_state =
        GetClientFilterableStateForVersion(current_version);
  }

  CreateTrialsResult create_trials_result = {.applied_seed = false};
  if (!used_testing_config && client_filterable_state) {
    create_trials_result = CreateTrialsFromSeed(
        entropy_providers, feature_list.get(), safe_seed_manager,
        std::move(client_filterable_state));
  }

  if (create_trials_result.applied_seed) {
    FieldTrialsProvider::UpdateAppliedSeedHasActiveLimitedLayer(
        create_trials_result.seed_has_active_limited_layer.value_or(false));
  }

  if (add_entropy_source_to_variations_ids &&
      !create_trials_result.AppliedSeedHasActiveLimitedLayer()) {
    // TODO(crbug.com/424154785): Consider no longer transmitting LES values
    // alongside VariationsIDs.
    http_header_provider->SetLowEntropySourceValue(
        metrics_state_manager->GetLowEntropySource());
  }

  platform_field_trials->SetUpClientSideFieldTrials(
      create_trials_result.applied_seed, entropy_providers, feature_list.get());

  platform_field_trials->RegisterFeatureOverrides(feature_list.get());

  base::FeatureList::SetInstance(std::move(feature_list));

  if (base::FeatureList::IsEnabled(internal::kPurgeVariationsSeedFromMemory)) {
    GetSeedStore()->AllowToPurgeSeedsDataFromMemory();
  }

  // For testing Variations Safe Mode, maybe crash here.
  if (base::FeatureList::IsEnabled(kForceFieldTrialSetupCrashForTesting)) {
    // Terminate with a custom exit test code. See
    // VariationsSafeModeEndToEndBrowserTest.ExtendedSafeSeedEndToEnd.
    base::Process::TerminateCurrentProcessImmediately(0x7E57C0D3);
  }

  // TODO(crbug.com/458408055): Remove these once the experiments are over.
  MaybeActivateMetricsNoopTrial();
  MaybeActivateMetricsNoopTrial2();

  // This must be called after |local_state_| is initialized.
  platform_field_trials->OnVariationsSetupComplete();

  VLOG(1) << "VariationsSetupComplete";

  return create_trials_result.applied_seed;
}

std::unique_ptr<ClientFilterableState>
VariationsFieldTrialCreator::GetClientFilterableStateForVersion(
    const base::Version& version) {
  // Note that passing base::Unretained(client_) is safe here because |client_|
  // lives until Chrome exits.
  auto IsEnterpriseCallback = base::BindRepeating(
      &VariationsServiceClient::IsEnterprise, base::Unretained(client_));
  auto GoogleGroupsCallback = base::BindRepeating(
      &VariationsFieldTrialCreator::GetGoogleGroupsFromPrefs,
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
  // Update the stored permanent consistency country
  permanent_consistency_country_ = state->permanent_consistency_country;
  permanent_consistency_country_initialized_ = true;

  state->policy_restriction = GetVariationPolicyRestriction(local_state());
  state->is_sticky_activation_enabled = client_->IsStickyActivationEnabled();
  return state;
}

std::string VariationsFieldTrialCreator::LoadPermanentConsistencyCountry(
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
        LoadPermanentConsistencyCountryResult::kHasPermanentOverriddenCountry);
    return permanent_overridden_country;
  }

  const std::string stored_version_string =
      seed_store_->GetPermanentConsistencyVersion();
  const std::string stored_country =
      seed_store_->GetPermanentConsistencyCountry();
  const bool is_stored_info_emtpy =
      stored_version_string.empty() && stored_country.empty();
  const base::Version stored_version(stored_version_string);
  const bool is_stored_info_valid = !stored_version_string.empty() &&
                                    !stored_country.empty() &&
                                    stored_version.IsValid();

  // Determine if the version from the saved pref matches |version|.
  const bool does_version_match =
      is_stored_info_valid && version == stored_version;

  // Determine if the country in the saved pref matches the country in
  // |latest_country|.
  const bool does_country_match = is_stored_info_valid &&
                                  !latest_country.empty() &&
                                  stored_country == latest_country;

  // Record a histogram for how the saved pref value compares to the current
  // version and the country code in the variations seed.
  LoadPermanentConsistencyCountryResult result;
  if (is_stored_info_emtpy) {
    result = !latest_country.empty()
                 ? LoadPermanentConsistencyCountryResult::kNoPrefHasSeed
                 : LoadPermanentConsistencyCountryResult::kNoPrefNoSeed;
  } else if (!is_stored_info_valid) {
    result = !latest_country.empty()
                 ? LoadPermanentConsistencyCountryResult::kInvalidPrefHasSeed
                 : LoadPermanentConsistencyCountryResult::kInvalidPrefNoSeed;
  } else if (latest_country.empty()) {
    result =
        does_version_match
            ? LoadPermanentConsistencyCountryResult::kHasPrefNoSeedVersionEq
            : LoadPermanentConsistencyCountryResult::kHasPrefNoSeedVersionNeq;
  } else if (does_version_match) {
    result =
        does_country_match
            ? LoadPermanentConsistencyCountryResult::kHasBothVersionEqCountryEq
            : LoadPermanentConsistencyCountryResult::
                  kHasBothVersionEqCountryNeq;
  } else {
    result =
        does_country_match
            ? LoadPermanentConsistencyCountryResult::kHasBothVersionNeqCountryEq
            : LoadPermanentConsistencyCountryResult::
                  kHasBothVersionNeqCountryNeq;
  }
  base::UmaHistogramEnumeration(
      "Variations.LoadPermanentConsistencyCountryResult", result);

  // Use the stored country if one is available and was fetched since the last
  // time Chrome was updated.
  if (does_version_match) {
    return stored_country;
  }

  if (latest_country.empty()) {
    if (!is_stored_info_valid) {
      seed_store_->ClearPermanentConsistencyCountryAndVersion();
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

std::string VariationsFieldTrialCreator::GetPermanentConsistencyCountry()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(permanent_consistency_country_initialized_);

  return permanent_consistency_country_;
}

void VariationsFieldTrialCreator::StorePermanentCountry(
    const base::Version& version,
    const std::string& country) {
  seed_store_->SetPermanentConsistencyCountryAndVersion(country,
                                                        version.GetString());
}

void VariationsFieldTrialCreator::StoreVariationsOverriddenCountry(
    const std::string& country) {
  local_state()->SetString(prefs::kVariationsPermanentOverriddenCountry,
                           country);
  permanent_consistency_country_ = country;
  permanent_consistency_country_initialized_ = true;
}

void VariationsFieldTrialCreator::OverrideVariationsPlatform(
    Study::Platform platform_override) {
  platform_override_ = platform_override;
}

base::Time VariationsFieldTrialCreator::GetSeedFetchTime() {
  // TODO(crbug.com/40274989): Consider comparing the server-provided fetch time
  // with the network time.
  return seed_type_ == SeedType::kSafeSeed
             ? GetSeedStore()->GetSafeSeedFetchTime()
             : GetSeedStore()->GetLatestSeedFetchTime();
}

base::Time VariationsFieldTrialCreator::GetLatestSeedFetchTime() {
  return GetSeedStore()->GetLatestSeedFetchTime();
}

void VariationsFieldTrialCreator::OverrideCachedUIStrings() {
  DCHECK(ui::ResourceBundle::HasSharedInstance());

  ui::ResourceBundle* bundle = &ui::ResourceBundle::GetSharedInstance();
  bundle->CheckCanOverrideStringResources();

  for (auto const& it : overridden_strings_map_) {
    bundle->OverrideLocaleStringResource(it.first, it.second);
  }

  overridden_strings_map_.clear();
}

bool VariationsFieldTrialCreator::IsOverrideResourceMapEmpty() {
  return overridden_strings_map_.empty();
}

void VariationsFieldTrialCreator::OverrideUIString(
    uint32_t resource_hash,
    const std::u16string& str) {
  int resource_id = ui_string_overrider_.GetResourceIndex(resource_hash);
  if (resource_id == -1) {
    return;
  }

  // This function may be called before the resource bundle is initialized. So
  // we cache the UI strings and override them after the full browser starts.
  if (!ui::ResourceBundle::HasSharedInstance()) {
    overridden_strings_map_[resource_id] = str;
    return;
  }

  ui::ResourceBundle::GetSharedInstance().OverrideLocaleStringResource(
      resource_id, str);
}

Study::Platform VariationsFieldTrialCreator::GetPlatform() {
  if (platform_override_.has_value()) {
    return platform_override_.value();
  }
  return ClientFilterableState::GetCurrentPlatform();
}

Study::FormFactor VariationsFieldTrialCreator::GetCurrentFormFactor() {
  return client_->GetCurrentFormFactor();
}

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)
void VariationsFieldTrialCreator::ApplyFieldTrialTestingConfig(
    base::FeatureList* feature_list) {
  VLOG(1) << "Applying FieldTrialTestingConfig";
  // Note that passing base::Unretained(this) below is safe because the callback
  // is executed synchronously.
  AssociateDefaultFieldTrialConfig(
      base::BindRepeating(&VariationsFieldTrialCreator::OverrideUIString,
                          base::Unretained(this)),
      GetPlatform(), GetCurrentFormFactor(), feature_list);
}
#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

bool VariationsFieldTrialCreator::HasSeedExpired() {
  const base::Time fetch_time = GetSeedFetchTime();
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

bool VariationsFieldTrialCreator::IsSeedForFutureMilestone(
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
VariationsFieldTrialCreator::GetGoogleGroupsFromPrefs() {
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

CreateTrialsResult VariationsFieldTrialCreator::CreateTrialsFromSeed(
    const EntropyProviders& entropy_providers,
    base::FeatureList* feature_list,
    SafeSeedManager* safe_seed_manager,
    std::unique_ptr<ClientFilterableState> client_state) {
  TRACE_EVENT0("startup", "VariationsFieldTrialCreator::CreateTrialsFromSeed");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!create_trials_from_seed_called_);
  CHECK(client_);
  CHECK(client_state);
  create_trials_from_seed_called_ = true;

  base::TimeTicks start_time = base::TimeTicks::Now();

  base::UmaHistogramSparse("Variations.UserChannel", client_state->channel);
  base::UmaHistogramEnumeration("Variations.PolicyRestriction",
                                client_state->policy_restriction);

  seed_type_ = safe_seed_manager->GetSeedType();
  // If we have tried safe seed and we still get crashes, try null seed.
  if (seed_type_ == SeedType::kNullSeed) {
    RecordVariationsSeedUsage(SeedUsage::kNullSeedUsed);
    return CreateTrialsResult{.applied_seed = false};
  }

  VariationsSeed seed;

  std::string seed_data;              // Only set if not in safe mode.
  std::string base64_seed_signature;  // Only set if not in safe mode.
  const bool run_in_safe_mode = seed_type_ == SeedType::kSafeSeed;
  // TODO: crbug.com/445600380 - Check if we can avoid copying the seed data
  // when loading the seed.
  const bool seed_loaded =
      run_in_safe_mode
          ? GetSeedStore()->LoadSafeSeedSync(&seed, client_state.get())
          : GetSeedStore()->LoadSeedSync(&seed, &seed_data,
                                         &base64_seed_signature);
  if (!seed_loaded) {
    // If Chrome should run in safe mode but the safe seed was not successfully
    // loaded, then do not apply a seed. Fall back to client-side defaults.
    RecordVariationsSeedUsage(run_in_safe_mode
                                  ? SeedUsage::kUnloadableSafeSeedNotUsed
                                  : SeedUsage::kUnloadableRegularSeedNotUsed);
    return CreateTrialsResult{.applied_seed = false};
  }
  if (HasSeedExpired()) {
    RecordVariationsSeedUsage(run_in_safe_mode
                                  ? SeedUsage::kExpiredSafeSeedNotUsed
                                  : SeedUsage::kExpiredRegularSeedNotUsed);
    return CreateTrialsResult{.applied_seed = false};
  }
  if (IsSeedForFutureMilestone(/*is_safe_seed=*/run_in_safe_mode)) {
    RecordVariationsSeedUsage(
        run_in_safe_mode ? SeedUsage::kSafeSeedForFutureMilestoneNotUsed
                         : SeedUsage::kRegularSeedForFutureMilestoneNotUsed);
    return CreateTrialsResult{.applied_seed = false};
  }

  VariationsLayers layers(seed, entropy_providers);

  // The server is not expected to send a seed with misconfigured entropy. Just
  // in case there is an unexpected server-side bug and the entropy is
  // misconfigured, return early to skip assigning any trials from the seed.
  // Also, generate a crash report, so that the misconfigured seed can be
  // identified and rolled back.
  //
  // Note that `VariationsLayers` ensures that no limited-entropy-mode layer
  // is marked as active for clients without a limited entropy provider, which
  // is the case for clients on platforms, like Android WebView, that do not
  // support limited entropy randomization. For such clients,
  // `SeedHasMisconfiguredEntropy()`is always false.
  const MisconfiguredEntropyResult result =
      SeedHasMisconfiguredEntropy(*client_state, seed);
  if (result.is_misconfigured) {
    RecordVariationsSeedUsage(
        run_in_safe_mode ? SeedUsage::kMisconfiguredSafeSeedNotUsed
                         : SeedUsage::kMisconfiguredRegularSeedNotUsed);
    return CreateTrialsResult{.applied_seed = false};
  }
  SetSeedVersion(seed.version());
  RecordVariationsSeedUsage(run_in_safe_mode ? SeedUsage::kSafeSeedUsed
                                             : SeedUsage::kRegularSeedUsed);

  // Note that passing base::Unretained(this) below is safe because the callback
  // is executed synchronously. It is not possible to pass UIStringOverrider
  // directly to VariationsSeedProcessor (which is in components/variations and
  // not components/variations/service) as the variations component should not
  // depend on //ui/base.
  VariationsSeedProcessor(sticky_activation_manager_)
      .CreateTrialsFromSeed(
          seed, *client_state,
          base::BindRepeating(&VariationsFieldTrialCreator::OverrideUIString,
                              base::Unretained(this)),
          entropy_providers, layers, feature_list);
  sticky_activation_manager_.StartMonitoring();

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
        std::move(client_state), seed_store_->GetLatestSeedFetchTime());
  }

  base::UmaHistogramCounts1M("Variations.AppliedSeed.Size", seed_data.size());
#if BUILDFLAG(IS_WIN)
  base::UmaHistogramCounts10M("Variations.AppliedSeed.Size.V2",
                              seed_data.size());
#endif  // BUILDFLAG(IS_WIN)
  base::UmaHistogramTimes("Variations.SeedProcessingTime",
                          base::TimeTicks::Now() - start_time);
  return CreateTrialsResult{
      .applied_seed = true,
      .seed_has_active_limited_layer = result.seed_has_active_limited_layer};
}

void VariationsFieldTrialCreator::LoadSeedFromJsonFile(
    const base::FilePath& json_seed_path) {
  VLOG(1) << "Loading seed from JSON file:" << json_seed_path;
  JSONFileValueDeserializer file_deserializer(json_seed_path);
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> json_contents =
      file_deserializer.Deserialize(&error_code, &error_message);

  if (!json_contents) {
    client_->ExitWithMessage(base::StringPrintf("Failed to load \"%s\" %s (%i)",
                                       json_seed_path.AsUTF8Unsafe().c_str(),
                                       error_message.c_str(), error_code));
  }

  const base::Value* seed_data =
      json_contents->GetDict().Find(prefs::kVariationsCompressedSeed);
  const base::Value* seed_signature =
      json_contents->GetDict().Find(prefs::kVariationsSeedSignature);

  if (!seed_data || !seed_data->is_string()) {
    client_->ExitWithMessage(
        base::StrCat({"Missing or invalid seed data in contents of \"",
                      json_seed_path.AsUTF8Unsafe(), "\""}));
  }

  if (!seed_signature || !seed_signature->is_string()) {
    client_->ExitWithMessage(
        base::StrCat({"Missing or invalid seed signature in contents of \"",
                      json_seed_path.AsUTF8Unsafe(), "\""}));
  }

  // Set fail counters to 0 to make sure Chrome doesn't run in variations safe
  // mode. This ensures that Chrome won't use the safe seed.
  local_state()->SetInteger(prefs::kVariationsCrashStreak, 0);
  local_state()->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);

  // Override Local State seed prefs.
  std::string decoded_seed;
  if (!base::Base64Decode(seed_data->GetString(), &decoded_seed)) {
    client_->ExitWithMessage(
        base::StrCat({"Failed to decode seed data in contents of \"",
                      json_seed_path.AsUTF8Unsafe(), "\""}));
  }
  seed_store_->StoreSeedData(/*done_callback=*/base::DoNothing(), decoded_seed,
                             seed_signature->GetString(), /*country_code=*/"",
                             /*date_fetched=*/base::Time(),
                             /*is_delta_compressed=*/false,
                             /*is_gzip_compressed=*/true,
                             /*require_synchronous=*/true);
}

VariationsSeedStore* VariationsFieldTrialCreator::GetSeedStore() {
  return seed_store_.get();
}

}  // namespace variations
