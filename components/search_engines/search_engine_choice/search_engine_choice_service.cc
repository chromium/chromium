// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"

#include <inttypes.h>

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "components/country_codes/country_codes.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_metrics_service_accessor.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/version_info/version_info.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/variations/service/variations_service.h"  // nogncheck
#endif

using ::country_codes::CountryId;

namespace search_engines {
namespace {

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
      BUILDFLAG(CHROME_FOR_TESTING))
// The choice screen should be shown if the `DefaultSearchProviderEnabled`
// policy is not set, or set to true and the
// `DefaultSearchProviderSearchURL` policy is not set.
bool IsSearchEngineChoiceScreenAllowedByPolicy(
    const policy::PolicyService& policy_service) {
  const auto& policies = policy_service.GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

  const auto* default_search_provider_enabled = policies.GetValue(
      policy::key::kDefaultSearchProviderEnabled, base::Value::Type::BOOLEAN);
  // Policy is not set.
  if (!default_search_provider_enabled) {
    return true;
  }

  if (default_search_provider_enabled->GetBool()) {
    const auto* default_search_provider_search_url =
        policies.GetValue(policy::key::kDefaultSearchProviderSearchURL,
                          base::Value::Type::STRING);
    if (!default_search_provider_search_url) {
      return true;
    }
  }
  return false;
}

bool IsSetOrBlockedByPolicy(const TemplateURL* default_search_engine) {
  return !default_search_engine ||
         default_search_engine->CreatedByDefaultSearchProviderPolicy();
}

bool IsDefaultSearchProviderSetOrBlockedByPolicy(
    const TemplateURLService& template_url_service) {
  return IsSetOrBlockedByPolicy(
      template_url_service.GetDefaultSearchProvider());
}
#endif

SearchEngineType GetDefaultSearchEngineType(
    TemplateURLService& template_url_service) {
  const TemplateURL* default_search_engine =
      template_url_service.GetDefaultSearchProvider();

  return default_search_engine ? default_search_engine->GetEngineType(
                                     template_url_service.search_terms_data())
                               : SEARCH_ENGINE_OTHER;
}

// Returns true if all search engine choice prefs are set.
bool IsSearchEngineChoiceCompleted(const PrefService& prefs) {
  return GetChoiceCompletionMetadata(prefs).has_value();
}

void MarkSearchEngineChoiceCompleted(PrefService& prefs) {
  SetChoiceCompletionMetadata(prefs, ChoiceCompletionMetadata{
                                         .timestamp = base::Time::Now(),
                                         .version = version_info::GetVersion(),
                                     });
}

// Returns true if the version is valid and can be compared to the current
// Chrome version.
bool IsValidVersionFormat(const base::Version& version) {
  if (!version.IsValid()) {
    return false;
  }

  // The version should have the same number of components as the current Chrome
  // version.
  if (version.components().size() !=
      version_info::GetVersion().components().size()) {
    return false;
  }
  return true;
}

// Logs the outcome of a reprompt attempt for a specific key (either a specific
// country or the wildcard).
void LogSearchRepromptKeyHistograms(RepromptResult result, bool is_wildcard) {
  // `RepromptResult::kInvalidDictionary` and `RepromptResult::kNoReprompt` are
  // recorded separately.
  CHECK_NE(result, RepromptResult::kInvalidDictionary);
  CHECK_NE(result, RepromptResult::kNoReprompt);

  base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptHistogram, result);
  if (is_wildcard) {
    base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptWildcardHistogram,
                                  result);
  } else {
    base::UmaHistogramEnumeration(
        kSearchEngineChoiceRepromptSpecificCountryHistogram, result);
  }
}

bool ShouldRepromptFromFeatureParams(
    const base::Version& persisted_choice_version,
    const CountryId& profile_country_id) {
  // Check parameters from `switches::kSearchEngineChoiceTriggerRepromptParams`.
  const std::string reprompt_params =
      switches::kSearchEngineChoiceTriggerRepromptParams.Get();
  if (reprompt_params == switches::kSearchEngineChoiceNoRepromptString) {
    base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptHistogram,
                                  RepromptResult::kNoReprompt);
    return false;
  }

  std::optional<base::Value::Dict> reprompt_params_json =
      base::JSONReader::ReadDict(reprompt_params);
  // Not a valid JSON.
  if (!reprompt_params_json) {
    base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptHistogram,
                                  RepromptResult::kInvalidDictionary);
    return false;
  }

  const base::Version& current_version = version_info::GetVersion();
  const std::string wildcard_string("*");
  // Explicit country key takes precedence over the wildcard.
  for (const std::string& key :
       {std::string(profile_country_id.CountryCode()), wildcard_string}) {
    bool is_wildcard = key == wildcard_string;
    const std::string* reprompt_version_string =
        reprompt_params_json->FindString(key);
    if (!reprompt_version_string) {
      // No version string for this country. Fallback to the wildcard.
      LogSearchRepromptKeyHistograms(RepromptResult::kNoDictionaryKey,
                                     is_wildcard);
      continue;
    }

    base::Version reprompt_version(*reprompt_version_string);
    if (!IsValidVersionFormat(reprompt_version)) {
      // The version is ill-formatted.
      LogSearchRepromptKeyHistograms(RepromptResult::kInvalidVersion,
                                     is_wildcard);
      break;
    }

    // Do not reprompt if the current version is too old, to avoid endless
    // reprompts.
    if (current_version < reprompt_version) {
      LogSearchRepromptKeyHistograms(RepromptResult::kChromeTooOld,
                                     is_wildcard);
      break;
    }

    if (persisted_choice_version >= reprompt_version) {
      // No need to reprompt, the choice is recent enough.
      LogSearchRepromptKeyHistograms(RepromptResult::kRecentChoice,
                                     is_wildcard);
      break;
    }

    // Wipe the choice to force a reprompt.
    LogSearchRepromptKeyHistograms(RepromptResult::kReprompt, is_wildcard);
    return true;
  }

  return false;
}

// Writes the histogram that tracks choice screen completion date in a specific
// format: YYYYMM (of type int).
void RecordChoiceScreenCompletionDate(PrefService& profile_prefs) {
  std::optional<base::Time> timestamp =
      GetChoiceScreenCompletionTimestamp(profile_prefs);
  if (!timestamp.has_value()) {
    return;
  }

  // Take year and month in local time.
  base::Time::Exploded exploded;
  timestamp->LocalExplode(&exploded);

  // Expected value space is 12 samples / year.
  base::UmaHistogramSparse(kSearchEngineChoiceCompletedOnMonthHistogram,
                           exploded.year * 100 + exploded.month);
}

}  // namespace

// -- SearchEngineChoiceService::Client ---------------------------------------

SearchEngineChoiceService::Client::~Client() = default;

// static
CountryId SearchEngineChoiceService::Client::GetVariationsLatestCountry(
    variations::VariationsService* variations_service) {
#if BUILDFLAG(IS_FUCHSIA)
  // We can't add a dependency from Fuchsia to
  // `//components/variations/service`.
  return CountryId();
#else
  return variations_service ? CountryId(base::ToUpperASCII(
                                  variations_service->GetLatestCountry()))
                            : CountryId();
#endif
}

// -- SearchEngineChoiceService -----------------------------------------------

SearchEngineChoiceService::SearchEngineChoiceService(
    std::unique_ptr<Client> client,
    PrefService& profile_prefs,
    PrefService* local_state,
    regional_capabilities::RegionalCapabilitiesService& regional_capabilities,
    TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver)
    : client_(std::move(client)),
      profile_prefs_(profile_prefs),
      local_state_(local_state),
      regional_capabilities_service_(regional_capabilities),
      prepopulate_data_resolver_(prepopulate_data_resolver) {
  ProcessPendingChoiceScreenDisplayState();
  PreprocessPrefsForReprompt();
  RecordChoiceScreenCompletionDate(profile_prefs);
}

SearchEngineChoiceService::~SearchEngineChoiceService() = default;

SearchEngineChoiceScreenConditions
SearchEngineChoiceService::GetStaticChoiceScreenConditions(
    const policy::PolicyService& policy_service,
    bool is_regular_profile,
    const TemplateURLService& template_url_service) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  // TODO(b/319050536): Remove the function declaration on these platforms.
  return SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
#else
  if (!is_regular_profile) {
    // Naming not exactly accurate, but still reflect the fact that incognito,
    // kiosk, etc. are not supported and belongs in this bucked more than in
    // `kProfileOutOfScope` for example.
    return SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
  }

  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  // A command line argument with the option for disabling the choice screen for
  // testing and automation environments.
  if (command_line->HasSwitch(switches::kDisableSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kFeatureSuppressed;
  }

  if (IsSearchEngineChoiceCompleted(*profile_prefs_)) {
    return SearchEngineChoiceScreenConditions::kAlreadyCompleted;
  }

  if (!regional_capabilities_service_->IsInEeaCountry()) {
    return SearchEngineChoiceScreenConditions::kNotInRegionalScope;
  }

  // Initially exclude users with this type of override. Consult b/302675777 for
  // next steps.
  if (profile_prefs_->HasPrefPath(prefs::kSearchProviderOverrides)) {
    return SearchEngineChoiceScreenConditions::kSearchProviderOverride;
  }

  if (!IsSearchEngineChoiceScreenAllowedByPolicy(policy_service) ||
      IsDefaultSearchProviderSetOrBlockedByPolicy(template_url_service)) {
    return SearchEngineChoiceScreenConditions::kControlledByPolicy;
  }

  return SearchEngineChoiceScreenConditions::kEligible;
#endif
}

SearchEngineChoiceScreenConditions
SearchEngineChoiceService::GetDynamicChoiceScreenConditions(
    const TemplateURLService& template_url_service) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  // TODO(b/319050536): Remove the function declaration on these platforms.
  return SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
#else
  // Don't show the dialog if the choice has already been made.
  if (IsSearchEngineChoiceCompleted(*profile_prefs_)) {
    return SearchEngineChoiceScreenConditions::kAlreadyCompleted;
  }

  // Don't show the dialog if the default search engine is set by an extension.
  if (template_url_service.IsExtensionControlledDefaultSearch()) {
    return SearchEngineChoiceScreenConditions::kExtensionControlled;
  }

  const TemplateURL* default_search_engine =
      template_url_service.GetDefaultSearchProvider();
  if (IsSetOrBlockedByPolicy(default_search_engine)) {
    // It is possible that between the static checks at service creation (around
    // the time the profile was loaded) and the moment a compatible URL is
    // loaded to show the search engine choice dialog, some new policies come in
    // and take control of the default search provider. If we proceeded here,
    // the choice screen could be shown and we might attempt to set a DSE based
    // on the user selection, but that would be ignored.
    return SearchEngineChoiceScreenConditions::kControlledByPolicy;
  }
  CHECK(default_search_engine);

  if (!IsSearchEngineChoiceInvalid(profile_prefs_.get()) &&
      default_search_engine->GetEngineType(
          template_url_service.search_terms_data()) != SEARCH_ENGINE_GOOGLE) {
    return SearchEngineChoiceScreenConditions::kHasNonGoogleSearchEngine;
  }

  if (!template_url_service.IsPrepopulatedOrDefaultProviderByPolicy(
          default_search_engine)) {
    return SearchEngineChoiceScreenConditions::kHasCustomSearchEngine;
  }

  if (default_search_engine->prepopulate_id() >
      TemplateURLPrepopulateData::kMaxPrepopulatedEngineID) {
    // Don't show a choice screen when the user has a distribution custom search
    // engine as default (they have prepopulate ID > 1000).
    // TODO(crbug.com/324880292): Revisit how those are handled.
    return SearchEngineChoiceScreenConditions::
        kHasDistributionCustomSearchEngine;
  }

  if (!prepopulate_data_resolver_->GetEngineFromFullList(
          default_search_engine->prepopulate_id())) {
    // The current default search engine was at some point part of the
    // prepopulated data (it has a "normal"-looking ID), but it has since been
    // removed. Follow what we do for custom search engines, don't show the
    // choice screen.
    return SearchEngineChoiceScreenConditions::
        kHasRemovedPrepopulatedSearchEngine;
  }

  return SearchEngineChoiceScreenConditions::kEligible;
#endif
}

std::unique_ptr<search_engines::ChoiceScreenData>
SearchEngineChoiceService::GetChoiceScreenData(
    const SearchTermsData& search_terms_data) {
  TemplateURLService::OwnedTemplateURLVector owned_template_urls;

  // We call `GetPrepopulatedEngines` instead of
  // `GetSearchProvidersUsingLoadedEngines` because the latter will return the
  // list of search engines that might have been modified by the user (by
  // changing the engine's keyword in settings for example).
  // Changing this will cause issues in the icon generation behavior that's
  // handled by `generate_search_engine_icons.py`.
  std::vector<std::unique_ptr<TemplateURLData>> engines =
      prepopulate_data_resolver_->GetPrepopulatedEngines();
  for (const auto& engine : engines) {
    owned_template_urls.push_back(std::make_unique<TemplateURL>(*engine));
  }

  return std::make_unique<search_engines::ChoiceScreenData>(
      std::move(owned_template_urls),
      regional_capabilities_service_->GetCountryId().GetRestricted(
          regional_capabilities::CountryAccessKey(
              regional_capabilities::CountryAccessReason::
                  kSearchEngineChoiceServiceCacheChoiceScreenData)),
      search_terms_data);
}

void SearchEngineChoiceService::RecordChoiceMade(
    ChoiceMadeLocation choice_location,
    TemplateURLService* template_url_service) {
  CHECK_NE(choice_location, ChoiceMadeLocation::kOther);

  ClearSearchEngineChoiceInvalidation(*profile_prefs_);

  // Don't modify the pref if the user is not in the EEA region.
  if (!regional_capabilities_service_->IsInEeaCountry()) {
    return;
  }

  // Don't modify the prefs if they were already set.
  if (IsSearchEngineChoiceCompleted(*profile_prefs_)) {
    return;
  }

  RecordChoiceScreenDefaultSearchProviderType(
      GetDefaultSearchEngineType(CHECK_DEREF(template_url_service)),
      choice_location);
  MarkSearchEngineChoiceCompleted(*profile_prefs_);
}

void SearchEngineChoiceService::MaybeRecordChoiceScreenDisplayState(
    const ChoiceScreenDisplayState& display_state,
    bool is_from_cached_state) {
  if (!regional_capabilities::IsEeaCountry(display_state.country_id)) {
    // Tests or command line can force this, but we want to avoid polluting the
    // histograms with unwanted country data.
    return;
  }

  // This block adds some debugging data for b/344899110, where the method
  // is called from the choice moment while a display state is already cached.
  // TODO(b/344899110): Clean up the debugging info when the bug is fixed.
  if (!is_from_cached_state) {
    if (!display_state_record_caller_) {
      CHECK(!profile_prefs_->HasPrefPath(
          prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
      display_state_record_caller_ =
          std::make_unique<base::debug::StackTrace>();
    } else {
      // Recording a stack trace to crash keys, based on
      // https://crsrc.org/c/docs/debugging_with_crash_keys.md
      static crash_reporter::CrashKeyString<1024> caller_trace_key(
          "ChoiceService-og_caller_trace");
      crash_reporter::SetCrashKeyStringToStackTrace(
          &caller_trace_key, *display_state_record_caller_.get());

      SCOPED_CRASH_KEY_BOOL(
          "ChoiceService", "ds_pref_has_value",
          profile_prefs_->HasPrefPath(
              prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

      std::optional<ChoiceScreenDisplayState> already_cached_display_state =
          ChoiceScreenDisplayState::FromDict(profile_prefs_->GetDict(
              prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
      std::optional<base::Time> completion_time =
          GetChoiceScreenCompletionTimestamp(profile_prefs_.get());

      SCOPED_CRASH_KEY_STRING64(
          "ChoiceService", "choice_time_delta",
          completion_time.has_value()
              ? base::StringPrintf("%" PRId64 "ms",
                                   (base::Time::Now() - completion_time.value())
                                       .InMilliseconds())
              : "<null>");
      SCOPED_CRASH_KEY_STRING32(
          "ChoiceService", "screen_items_equal",
          already_cached_display_state.has_value()
              ? (already_cached_display_state.value().search_engines ==
                         display_state.search_engines
                     ? "yes"
                     : "no")
              : "no value");

      NOTREACHED(base::NotFatalUntil::M141);
      caller_trace_key.Clear();
    }
  }

  if (!is_from_cached_state &&
      display_state.selected_engine_index.has_value()) {
    RecordChoiceScreenSelectedIndex(
        display_state.selected_engine_index.value());
  }

  if (display_state.country_id != client_->GetVariationsCountry()) {
    // Not recording if adding position data, which can be used as a proxy for
    // the profile country, would add new hard to control location info to a
    // logs session.
    if (!is_from_cached_state) {
      // Persist the data so we can attempt to send it later.
      RecordChoiceScreenPositionsCountryMismatch(true);
      profile_prefs_->SetDict(
          prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
          display_state.ToDict());
    }
    return;
  }

  RecordChoiceScreenPositions(display_state.search_engines);
  if (is_from_cached_state) {
    profile_prefs_->ClearPref(
        prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState);
  } else {
    RecordChoiceScreenPositionsCountryMismatch(false);
  }
}

void SearchEngineChoiceService::PreprocessPrefsForReprompt() {
  base::expected<ChoiceCompletionMetadata, ChoiceCompletionMetadata::ParseError>
      completion_metadata = GetChoiceCompletionMetadata(profile_prefs_.get());
  if (!completion_metadata.has_value()) {
    switch (completion_metadata.error()) {
      case ChoiceCompletionMetadata::ParseError::kAbsent:
        // No choice has been made at all, so there is nothing to reset.
        return;
      case ChoiceCompletionMetadata::ParseError::kMissingVersion:
        WipeSearchEngineChoicePrefs(
            profile_prefs_.get(),
            SearchEngineChoiceWipeReason::kMissingMetadataVersion);
        return;
      case ChoiceCompletionMetadata::ParseError::kInvalidVersion:
        WipeSearchEngineChoicePrefs(
            profile_prefs_.get(),
            SearchEngineChoiceWipeReason::kInvalidMetadataVersion);
        return;
      case ChoiceCompletionMetadata::ParseError::kOther:
        WipeSearchEngineChoicePrefs(
            profile_prefs_.get(),
            SearchEngineChoiceWipeReason::kInvalidMetadata);
        return;
    }
  }

  // Allow re-triggering the choice screen for testing the screen itself.
  // This flag is deliberately only clearing the prefs instead of more
  // forcefully triggering the screen because this allows to more easily test
  // the flows without risking to put the choice screens in some unstable state.
  // The other conditions (e.g. country, policies, etc) are rather easy to
  // change if we want to re-enable the triggering.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceSearchEngineChoiceScreen)) {
    WipeSearchEngineChoicePrefs(profile_prefs_.get(),
                                SearchEngineChoiceWipeReason::kCommandLineFlag);
    return;
  }

  if (base::FeatureList::IsEnabled(
          switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection) &&
      client_->DoesChoicePredateDeviceRestore(completion_metadata.value())) {
    if (switches::kInvalidateChoiceOnRestoreIsRetroactive.Get() ||
        client_->IsDeviceRestoreDetectedInCurrentSession()) {
      WipeSearchEngineChoicePrefs(
          profile_prefs_.get(), SearchEngineChoiceWipeReason::kDeviceRestored);
      return;
    }
  }

  if (ShouldRepromptFromFeatureParams(
          completion_metadata->version,
          regional_capabilities_service_->GetCountryId().GetRestricted(
              regional_capabilities::CountryAccessKey(
                  regional_capabilities::CountryAccessReason::
                      kSearchEngineChoiceServiceReprompting)))) {
    WipeSearchEngineChoicePrefs(
        profile_prefs_.get(),
        SearchEngineChoiceWipeReason::kFinchBasedReprompt);
  }
}

void SearchEngineChoiceService::ProcessPendingChoiceScreenDisplayState() {
  if (!profile_prefs_->HasPrefPath(
          prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState)) {
    return;
  }

  if (!local_state_) {
    // `g_browser_process->local_state()` is null in unit tests unless properly
    // set up.
    CHECK_IS_TEST();
  } else if (!SearchEngineChoiceMetricsServiceAccessor::
                 IsMetricsReportingEnabled(local_state_)) {
    // The display state should not be cached when UMA is disabled.

    profile_prefs_->ClearPref(
        prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState);
    return;
  }

  const base::Value::Dict& dict = profile_prefs_->GetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState);
  std::optional<ChoiceScreenDisplayState> display_state =
      ChoiceScreenDisplayState::FromDict(dict);
  if (display_state.has_value()) {
    // Check if the obtained display state is still valid.
    std::optional<base::Time> completion_time =
        GetChoiceScreenCompletionTimestamp(profile_prefs_.get());
    constexpr base::TimeDelta kDisplayStateMaxPendingDuration = base::Days(7);
    if (base::Time::Now() - completion_time.value_or(base::Time::Min()) >
        kDisplayStateMaxPendingDuration) {
      display_state = std::nullopt;
    }
  }

  if (!display_state.has_value()) {
    profile_prefs_->ClearPref(
        prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState);
    return;
  }

  MaybeRecordChoiceScreenDisplayState(display_state.value(),
                                      /*is_from_cached_state=*/true);
}

void SearchEngineChoiceService::ResetState() {
  display_state_record_caller_.reset();
}

// static
void SearchEngineChoiceService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  registry->RegisterInt64Pref(
      prefs::kDefaultSearchProviderGuestModePrepopulatedId, 0);
#endif
}

void SearchEngineChoiceService::ClearCountryIdCacheForTesting() {
  CHECK_IS_TEST();
  regional_capabilities_service_->ClearCountryIdCacheForTesting();  // IN-TEST
}

SearchEngineChoiceService::Client&
SearchEngineChoiceService::GetClientForTesting() {
  CHECK_IS_TEST();
  return *client_.get();
}

bool SearchEngineChoiceService::IsDsePropagationAllowedForGuest() const {
  if (client_->IsProfileEligibleForDseGuestPropagation()) {
    return regional_capabilities_service_->IsInEeaCountry();
  }
  return false;
}

std::optional<int>
SearchEngineChoiceService::GetSavedSearchEngineBetweenGuestSessions() const {
  if (!IsDsePropagationAllowedForGuest()) {
    return std::nullopt;
  }
  if (local_state_->HasPrefPath(
          prefs::kDefaultSearchProviderGuestModePrepopulatedId)) {
    return local_state_->GetInt64(
        prefs::kDefaultSearchProviderGuestModePrepopulatedId);
  } else {
    return std::nullopt;
  }
}

void SearchEngineChoiceService::SetSavedSearchEngineBetweenGuestSessions(
    std::optional<int> prepopulated_id) {
  CHECK(!prepopulated_id.has_value() ||
        (prepopulated_id > 0 &&
         prepopulated_id <=
             TemplateURLPrepopulateData::kMaxPrepopulatedEngineID));
  CHECK(IsDsePropagationAllowedForGuest());

  if (prepopulated_id == GetSavedSearchEngineBetweenGuestSessions()) {
    return;
  }

  if (prepopulated_id.has_value()) {
    local_state_->SetInt64(prefs::kDefaultSearchProviderGuestModePrepopulatedId,
                           *prepopulated_id);
  } else {
    local_state_->ClearPref(
        prefs::kDefaultSearchProviderGuestModePrepopulatedId);
  }
  observers_.Notify(&Observer::OnSavedGuestSearchChanged);
}

// static
void MarkSearchEngineChoiceCompletedForTesting(PrefService& prefs) {
  CHECK_IS_TEST();
  MarkSearchEngineChoiceCompleted(prefs);
}

}  // namespace search_engines
