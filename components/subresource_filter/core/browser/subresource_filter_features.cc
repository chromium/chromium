// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/subresource_filter_features.h"

#include <algorithm>
#include <map>
#include <ostream>
#include <sstream>
#include <tuple>
#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

namespace subresource_filter {

namespace {

// Helpers --------------------------------------------------------------------

class CommaSeparatedStrings {
 public:
  explicit CommaSeparatedStrings(std::string comma_separated_strings)
      : backing_string_(comma_separated_strings),
        pieces_(base::SplitStringPiece(backing_string_,
                                       ",",
                                       base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_NONEMPTY)) {}

  bool CaseInsensitiveContains(base::StringPiece lowercase_key) const {
    const auto predicate = [lowercase_key](base::StringPiece element) {
      return base::LowerCaseEqualsASCII(element, lowercase_key);
    };
    return std::find_if(pieces_.begin(), pieces_.end(), predicate) !=
           pieces_.end();
  }

 private:
  const std::string backing_string_;
  const std::vector<base::StringPiece> pieces_;

  DISALLOW_COPY_AND_ASSIGN(CommaSeparatedStrings);
};

std::string TakeVariationParamOrReturnEmpty(
    std::map<std::string, std::string>* params,
    const std::string& key) {
  auto it = params->find(key);
  if (it == params->end())
    return std::string();
  std::string value = std::move(it->second);
  params->erase(it);
  return value;
}

mojom::ActivationLevel ParseActivationLevel(
    const base::StringPiece activation_level) {
  if (base::LowerCaseEqualsASCII(activation_level, kActivationLevelEnabled))
    return mojom::ActivationLevel::kEnabled;
  else if (base::LowerCaseEqualsASCII(activation_level, kActivationLevelDryRun))
    return mojom::ActivationLevel::kDryRun;
  return mojom::ActivationLevel::kDisabled;
}

ActivationScope ParseActivationScope(const base::StringPiece activation_scope) {
  if (base::LowerCaseEqualsASCII(activation_scope, kActivationScopeAllSites))
    return ActivationScope::ALL_SITES;
  else if (base::LowerCaseEqualsASCII(activation_scope,
                                      kActivationScopeActivationList))
    return ActivationScope::ACTIVATION_LIST;
  return ActivationScope::NO_SITES;
}

ActivationList ParseActivationList(std::string activation_lists_string) {
  CommaSeparatedStrings activation_lists(std::move(activation_lists_string));
  if (activation_lists.CaseInsensitiveContains(
          kActivationListPhishingInterstitial)) {
    return ActivationList::PHISHING_INTERSTITIAL;
  } else if (activation_lists.CaseInsensitiveContains(
                 kActivationListSocialEngineeringAdsInterstitial)) {
    return ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL;
  } else if (activation_lists.CaseInsensitiveContains(
                 kActivationListSubresourceFilter)) {
    return ActivationList::SUBRESOURCE_FILTER;
  } else if (activation_lists.CaseInsensitiveContains(
                 kActivationListBetterAds)) {
    return ActivationList::BETTER_ADS;
  }
  return ActivationList::NONE;
}

// Will return a value between 0 and 1 inclusive.
double ParsePerformanceMeasurementRate(const std::string& rate) {
  double value = 0.0;
  if (!base::StringToDouble(rate, &value) || value < 0)
    return 0.0;
  return value < 1 ? value : 1;
}

int ParseInt(const base::StringPiece value) {
  int result = 0;
  base::StringToInt(value, &result);
  return result;
}

std::vector<Configuration> FillEnabledPresetConfigurations(
    std::map<std::string, std::string>* params) {
  // If ad tagging is enabled, turn on the dryrun automatically.
  bool ad_tagging_enabled = base::FeatureList::IsEnabled(kAdTagging);
  const struct {
    const char* name;
    bool enabled_by_default;
    Configuration (*factory_method)();
  } kAvailablePresetConfigurations[] = {
      {kPresetLiveRunOnPhishingSites, true,
       &Configuration::MakePresetForLiveRunOnPhishingSites},
      {kPresetPerformanceTestingDryRunOnAllSites, ad_tagging_enabled,
       &Configuration::MakePresetForPerformanceTestingDryRunOnAllSites},
      {kPresetLiveRunForBetterAds, true,
       &Configuration::MakePresetForLiveRunForBetterAds}};

  CommaSeparatedStrings enabled_presets(
      TakeVariationParamOrReturnEmpty(params, kEnablePresetsParameterName));
  CommaSeparatedStrings disabled_presets(
      TakeVariationParamOrReturnEmpty(params, kDisablePresetsParameterName));

  std::vector<Configuration> enabled_configurations;
  for (const auto& available_preset : kAvailablePresetConfigurations) {
    if ((enabled_presets.CaseInsensitiveContains(available_preset.name) ||
         available_preset.enabled_by_default) &&
        !disabled_presets.CaseInsensitiveContains(available_preset.name)) {
      enabled_configurations.push_back(available_preset.factory_method());
    }
  }

  return enabled_configurations;
}

Configuration ParseExperimentalConfiguration(
    std::map<std::string, std::string>* params) {
  Configuration configuration;

  // ActivationConditions:
  configuration.activation_conditions.activation_scope = ParseActivationScope(
      TakeVariationParamOrReturnEmpty(params, kActivationScopeParameterName));

  configuration.activation_conditions.activation_list = ParseActivationList(
      TakeVariationParamOrReturnEmpty(params, kActivationListsParameterName));

  configuration.activation_conditions.priority =
      ParseInt(TakeVariationParamOrReturnEmpty(
          params, kActivationPriorityParameterName));

  // ActivationOptions:
  configuration.activation_options.activation_level = ParseActivationLevel(
      TakeVariationParamOrReturnEmpty(params, kActivationLevelParameterName));

  configuration.activation_options.performance_measurement_rate =
      ParsePerformanceMeasurementRate(TakeVariationParamOrReturnEmpty(
          params, kPerformanceMeasurementRateParameterName));

  // GeneralSettings:
  configuration.general_settings.ruleset_flavor =
      TakeVariationParamOrReturnEmpty(params, kRulesetFlavorParameterName);

  return configuration;
}

std::vector<Configuration> ParseEnabledConfigurations() {
  std::map<std::string, std::string> params;
  base::GetFieldTrialParamsByFeature(kSafeBrowsingSubresourceFilter, &params);

  std::vector<Configuration> configs;
  if (base::FeatureList::IsEnabled(kSafeBrowsingSubresourceFilter))
    configs = FillEnabledPresetConfigurations(&params);

  Configuration experimental_config = ParseExperimentalConfiguration(&params);
  configs.push_back(std::move(experimental_config));

  return configs;
}

template <class T>
std::string StreamToString(const T& value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

std::vector<Configuration> SortConfigsByDecreasingPriority(
    std::vector<Configuration> configs) {
  auto comp = [](const Configuration& a, const Configuration& b) {
    return a.activation_conditions.priority > b.activation_conditions.priority;
  };
  std::sort(configs.begin(), configs.end(), comp);
  return configs;
}

base::StringPiece GetLexicographicallyGreatestRulesetFlavor(
    const std::vector<Configuration>& configs) {
  base::StringPiece greatest_flavor;
  for (const auto& config : configs) {
    base::StringPiece flavor = config.general_settings.ruleset_flavor;
    if (flavor > greatest_flavor)
      greatest_flavor = flavor;
  }
  return greatest_flavor;
}

// Globals --------------------------------------------------------------------

base::LazyInstance<base::Lock>::Leaky g_active_configurations_lock =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<scoped_refptr<ConfigurationList>>::Leaky
    g_active_configurations = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Constant definitions -------------------------------------------------------

const base::Feature kSafeBrowsingSubresourceFilter{
    "SubresourceFilter", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingSubresourceFilterConsiderRedirects{
    "SubresourceFilterConsiderRedirects", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kFilterAdsOnAbusiveSites{"FilterAdsOnAbusiveSites",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Legacy name `activation_state` is used in variation parameters.
const char kActivationLevelParameterName[] = "activation_state";
const char kActivationLevelDryRun[] = "dryrun";
const char kActivationLevelEnabled[] = "enabled";
const char kActivationLevelDisabled[] = "disabled";

const char kActivationScopeParameterName[] = "activation_scope";
const char kActivationScopeAllSites[] = "all_sites";
const char kActivationScopeActivationList[] = "activation_list";
const char kActivationScopeNoSites[] = "no_sites";

const char kActivationListsParameterName[] = "activation_lists";
const char kActivationListSocialEngineeringAdsInterstitial[] =
    "social_engineering_ads_interstitial";
const char kActivationListPhishingInterstitial[] = "phishing_interstitial";
const char kActivationListSubresourceFilter[] = "subresource_filter";
const char kActivationListBetterAds[] = "better_ads";

const char kActivationPriorityParameterName[] = "activation_priority";

const char kPerformanceMeasurementRateParameterName[] =
    "performance_measurement_rate";
const char kRulesetFlavorParameterName[] = "ruleset_flavor";

const char kEnablePresetsParameterName[] = "enable_presets";
const char kDisablePresetsParameterName[] = "disable_presets";
const char kPresetLiveRunOnPhishingSites[] = "liverun_on_phishing_sites";
const char kPresetPerformanceTestingDryRunOnAllSites[] =
    "performance_testing_dryrun_on_all_sites";
const char kPresetLiveRunForBetterAds[] =
    "liverun_on_better_ads_violating_sites";

// Configuration --------------------------------------------------------------

// static
Configuration Configuration::MakePresetForLiveRunOnPhishingSites() {
  Configuration config(mojom::ActivationLevel::kEnabled,
                       ActivationScope::ACTIVATION_LIST,
                       ActivationList::PHISHING_INTERSTITIAL);
  config.activation_conditions.priority = 1000;
  return config;
}

// static
Configuration Configuration::MakePresetForPerformanceTestingDryRunOnAllSites() {
  Configuration config(mojom::ActivationLevel::kDryRun,
                       ActivationScope::ALL_SITES);
  config.activation_options.performance_measurement_rate = 0.01;
  config.activation_conditions.priority = 500;
  return config;
}

// static
Configuration Configuration::MakePresetForLiveRunForBetterAds() {
  Configuration config(mojom::ActivationLevel::kEnabled,
                       ActivationScope::ACTIVATION_LIST,
                       ActivationList::BETTER_ADS);
  config.activation_conditions.priority = 800;
  return config;
}

Configuration::Configuration() = default;
Configuration::Configuration(mojom::ActivationLevel activation_level,
                             ActivationScope activation_scope,
                             ActivationList activation_list) {
  activation_options.activation_level = activation_level;
  activation_conditions.activation_scope = activation_scope;
  activation_conditions.activation_list = activation_list;
}
Configuration::Configuration(const Configuration&) = default;
Configuration::Configuration(Configuration&&) = default;
Configuration::~Configuration() = default;
Configuration& Configuration::operator=(const Configuration&) = default;
Configuration& Configuration::operator=(Configuration&&) = default;

bool Configuration::operator==(const Configuration& rhs) const {
  const auto tie = [](const Configuration& config) {
    return std::tie(config.activation_conditions.activation_scope,
                    config.activation_conditions.activation_list,
                    config.activation_conditions.priority,
                    config.activation_options.activation_level,
                    config.activation_options.performance_measurement_rate,
                    config.general_settings.ruleset_flavor);
  };
  return tie(*this) == tie(rhs);
}

bool Configuration::operator!=(const Configuration& rhs) const {
  return !(*this == rhs);
}

std::unique_ptr<base::trace_event::TracedValue>
Configuration::ActivationConditions::ToTracedValue() const {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  value->SetString("activation_scope", StreamToString(activation_scope));
  value->SetString("activation_list", StreamToString(activation_list));
  value->SetInteger("priority", priority);
  return value;
}

std::unique_ptr<base::trace_event::TracedValue> Configuration::ToTracedValue()
    const {
  auto value = std::make_unique<base::trace_event::TracedValue>();
  auto traced_conditions = activation_conditions.ToTracedValue();
  value->SetValue("activation_conditions", traced_conditions.get());
  value->SetString("activation_level",
                   StreamToString(activation_options.activation_level));
  value->SetDouble("performance_measurement_rate",
                   activation_options.performance_measurement_rate);
  value->SetString("ruleset_flavor",
                   StreamToString(general_settings.ruleset_flavor));
  return value;
}

mojom::ActivationState Configuration::GetActivationState(
    mojom::ActivationLevel effective_activation_level) const {
  mojom::ActivationState state;
  state.activation_level = effective_activation_level;

  double measurement_rate = activation_options.performance_measurement_rate;
  state.measure_performance =
      base::ThreadTicks::IsSupported() &&
      (measurement_rate == 1 || base::RandDouble() < measurement_rate);

  // This bit keeps track of BAS enforcement-style logging, not warning logging.
  // TODO(csharrison): Consider removing it since it can be computed directly
  // from the mojom::ActivationLevel.
  state.enable_logging =
      effective_activation_level == mojom::ActivationLevel::kEnabled;
  return state;
}

std::ostream& operator<<(std::ostream& os, const Configuration& config) {
  os << config.ToTracedValue()->ToString();
  return os;
}

// ConfigurationList ----------------------------------------------------------

ConfigurationList::ConfigurationList(std::vector<Configuration> configs)
    : configs_by_decreasing_priority_(
          SortConfigsByDecreasingPriority(std::move(configs))),
      lexicographically_greatest_ruleset_flavor_(
          GetLexicographicallyGreatestRulesetFlavor(
              configs_by_decreasing_priority_)) {}
ConfigurationList::~ConfigurationList() = default;

scoped_refptr<ConfigurationList> GetEnabledConfigurations() {
  base::AutoLock lock(g_active_configurations_lock.Get());
  if (!g_active_configurations.Get()) {
    g_active_configurations.Get() =
        base::MakeRefCounted<ConfigurationList>(ParseEnabledConfigurations());
  }
  return g_active_configurations.Get();
}

bool HasEnabledConfiguration(const Configuration& config) {
  return base::Contains(
      GetEnabledConfigurations()->configs_by_decreasing_priority(), config);
}

namespace testing {

scoped_refptr<ConfigurationList> GetAndSetActivateConfigurations(
    scoped_refptr<ConfigurationList> new_configs) {
  base::AutoLock lock(g_active_configurations_lock.Get());
  auto old_configs = std::move(g_active_configurations.Get());
  g_active_configurations.Get() = std::move(new_configs);
  return old_configs;
}

}  // namespace testing

}  // namespace subresource_filter
