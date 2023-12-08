// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"
#include <string>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "base/version.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace search_engines {
namespace {

// Logs the outcome of a reprompt attempt for a specific key (either a specific
// country or the wildcard).
void LogSearchRepromptKeyHistograms(RepromptResult result, bool is_wildcard) {
  // `RepromptResult::kInvalidDictionary` is recorded separately.
  CHECK_NE(result, RepromptResult::kInvalidDictionary);

  base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptHistogram, result);
  if (is_wildcard) {
    base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptWildcardHistogram,
                                  result);
  } else {
    base::UmaHistogramEnumeration(
        kSearchEngineChoiceRepromptSpecificCountryHistogram, result);
  }
}

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

base::flat_set<int> GetEeaChoiceCountries() {
  using country_codes::CountryCharsToCountryID;

  // Google-internal reference: http://go/geoscope-comparisons.
  return base::flat_set<int>({
      CountryCharsToCountryID('A', 'T'),  // Austria
      CountryCharsToCountryID('A', 'X'),  // Åland Islands
      CountryCharsToCountryID('B', 'E'),  // Belgium
      CountryCharsToCountryID('B', 'G'),  // Bulgaria
      CountryCharsToCountryID('B', 'L'),  // St. Barthélemy
      CountryCharsToCountryID('C', 'Y'),  // Cyprus
      CountryCharsToCountryID('C', 'Z'),  // Czech Republic
      CountryCharsToCountryID('D', 'E'),  // Germany
      CountryCharsToCountryID('D', 'K'),  // Denmark
      CountryCharsToCountryID('E', 'A'),  // Ceuta & Melilla
      CountryCharsToCountryID('E', 'E'),  // Estonia
      CountryCharsToCountryID('E', 'S'),  // Spain
      CountryCharsToCountryID('F', 'I'),  // Finland
      CountryCharsToCountryID('F', 'R'),  // France
      CountryCharsToCountryID('G', 'F'),  // French Guiana
      CountryCharsToCountryID('G', 'P'),  // Guadeloupe
      CountryCharsToCountryID('G', 'R'),  // Greece
      CountryCharsToCountryID('H', 'R'),  // Croatia
      CountryCharsToCountryID('H', 'U'),  // Hungary
      CountryCharsToCountryID('I', 'C'),  // Canary Islands
      CountryCharsToCountryID('I', 'E'),  // Ireland
      CountryCharsToCountryID('I', 'S'),  // Iceland
      CountryCharsToCountryID('I', 'T'),  // Italy
      CountryCharsToCountryID('L', 'I'),  // Liechtenstein
      CountryCharsToCountryID('L', 'T'),  // Lithuania
      CountryCharsToCountryID('L', 'U'),  // Luxembourg
      CountryCharsToCountryID('L', 'V'),  // Latvia
      CountryCharsToCountryID('M', 'F'),  // St. Martin
      CountryCharsToCountryID('M', 'Q'),  // Martinique
      CountryCharsToCountryID('M', 'T'),  // Malta
      CountryCharsToCountryID('N', 'C'),  // New Caledonia
      CountryCharsToCountryID('N', 'L'),  // Netherlands
      CountryCharsToCountryID('N', 'O'),  // Norway
      CountryCharsToCountryID('P', 'F'),  // French Polynesia
      CountryCharsToCountryID('P', 'L'),  // Poland
      CountryCharsToCountryID('P', 'M'),  // St. Pierre & Miquelon
      CountryCharsToCountryID('P', 'T'),  // Portugal
      CountryCharsToCountryID('R', 'E'),  // Réunion
      CountryCharsToCountryID('R', 'O'),  // Romania
      CountryCharsToCountryID('S', 'E'),  // Sweden
      CountryCharsToCountryID('S', 'I'),  // Slovenia
      CountryCharsToCountryID('S', 'J'),  // Svalbard & Jan Mayen
      CountryCharsToCountryID('S', 'K'),  // Slovakia
      CountryCharsToCountryID('T', 'F'),  // French Southern Territories
      CountryCharsToCountryID('V', 'A'),  // Vatican City
      CountryCharsToCountryID('W', 'F'),  // Wallis & Futuna
      CountryCharsToCountryID('Y', 'T'),  // Mayotte
  });
}

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
  return prefs.HasPrefPath(
             prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp) &&
         prefs.HasPrefPath(
             prefs::kDefaultSearchProviderChoiceScreenCompletionVersion);
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

}  // namespace

const char kSearchEngineChoiceScreenNavigationConditionsHistogram[] =
    "Search.ChoiceScreenNavigationConditions";

const char kSearchEngineChoiceScreenProfileInitConditionsHistogram[] =
    "Search.ChoiceScreenProfileInitConditions";

const char kSearchEngineChoiceScreenEventsHistogram[] =
    "Search.ChoiceScreenEvents";

const char kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram[] =
    "Search.ChoiceScreenDefaultSearchEngineType";

const char kSearchEngineChoiceWipeReasonHistogram[] = "Search.ChoiceWipeReason";

const char kSearchEngineChoiceRepromptHistogram[] = "Search.ChoiceReprompt";

const char kSearchEngineChoiceRepromptWildcardHistogram[] =
    "Search.ChoiceReprompt.Wildcard";

const char kSearchEngineChoiceRepromptSpecificCountryHistogram[] =
    "Search.ChoiceReprompt.SpecificCountry";

// Returns whether the choice screen flag is generally enabled for the specific
// user flow.
bool IsChoiceScreenFlagEnabled(ChoicePromo promo) {
  if (base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger)) {
#if BUILDFLAG(IS_IOS)
    // Chrome on iOS does not tag profiles, so this param instead determines
    // whether we show the choice screen outside of the FRE or not.
    if (switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.Get() &&
        promo == ChoicePromo::kDialog) {
      return false;
    }
#endif

    // This flag is a coordinating flag, which supersedes the flags below that
    // are guarding individual screens making up the feature.
    // TODO(b/310593464): Remove checks for the other flags.
    return true;
  }

  switch (promo) {
    case ChoicePromo::kAny:
      return base::FeatureList::IsEnabled(switches::kSearchEngineChoice) ||
             base::FeatureList::IsEnabled(switches::kSearchEngineChoiceFre);
    case ChoicePromo::kDialog:
      return base::FeatureList::IsEnabled(switches::kSearchEngineChoice);
    case ChoicePromo::kFre:
      return base::FeatureList::IsEnabled(switches::kSearchEngineChoiceFre);
  }
}

bool ShouldShowUpdatedSettings(PrefService& profile_prefs) {
  return IsChoiceScreenFlagEnabled(ChoicePromo::kAny) &&
         IsEeaChoiceCountry(GetSearchEngineChoiceCountryId(&profile_prefs));
}

bool ShouldShowChoiceScreen(const policy::PolicyService& policy_service,
                            const ProfileProperties& profile_properties,
                            TemplateURLService* template_url_service) {
  PreprocessPrefsForReprompt(*profile_properties.pref_service);
  auto condition = GetStaticChoiceScreenConditions(
      policy_service, profile_properties, CHECK_DEREF(template_url_service));

  if (condition == SearchEngineChoiceScreenConditions::kEligible) {
    condition = GetDynamicChoiceScreenConditions(
        *profile_properties.pref_service, *template_url_service);
  }

  RecordChoiceScreenProfileInitCondition(condition);
  return condition == SearchEngineChoiceScreenConditions::kEligible;
}

SearchEngineChoiceScreenConditions GetStaticChoiceScreenConditions(
    const policy::PolicyService& policy_service,
    const ProfileProperties& profile_properties,
    const TemplateURLService& template_url_service) {
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return SearchEngineChoiceScreenConditions::kFeatureSuppressed;
  }

  PrefService& prefs = CHECK_DEREF(profile_properties.pref_service.get());
  if (switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.Get() &&
      !prefs.GetBoolean(prefs::kDefaultSearchProviderChoicePending)) {
    return SearchEngineChoiceScreenConditions::kProfileOutOfScope;
  }

  if (!profile_properties.is_regular_profile) {
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

  // Force triggering the choice screen for testing the screen itself.
  if (command_line->HasSwitch(switches::kForceSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kEligible;
  }

  if (IsSearchEngineChoiceCompleted(prefs)) {
    return SearchEngineChoiceScreenConditions::kAlreadyCompleted;
  }

  int country_id = GetSearchEngineChoiceCountryId(&prefs);
  DVLOG(1) << "Checking country for choice screen, found: "
           << country_codes::CountryIDToCountryString(country_id);
  if (!IsEeaChoiceCountry(country_id)) {
    return SearchEngineChoiceScreenConditions::kNotInRegionalScope;
  }

  // Initially exclude users with this type of override. Consult b/302675777 for
  // next steps.
  if (prefs.HasPrefPath(prefs::kSearchProviderOverrides)) {
    return SearchEngineChoiceScreenConditions::kSearchProviderOverride;
  }

  if (!IsSearchEngineChoiceScreenAllowedByPolicy(policy_service)) {
    return SearchEngineChoiceScreenConditions::kControlledByPolicy;
  }

  return SearchEngineChoiceScreenConditions::kEligible;
}

SearchEngineChoiceScreenConditions GetDynamicChoiceScreenConditions(
    const PrefService& profile_prefs,
    const TemplateURLService& template_url_service) {
  // Don't show the dialog if the default search engine is set by an extension.
  if (template_url_service.IsExtensionControlledDefaultSearch()) {
    return SearchEngineChoiceScreenConditions::kExtensionControlled;
  }

  // Don't show the dialog if the user has a custom search engine set as
  // default.
  const TemplateURL* default_search_engine =
      template_url_service.GetDefaultSearchProvider();
  if (default_search_engine &&
      !template_url_service.IsPrepopulatedOrDefaultProviderByPolicy(
          default_search_engine)) {
    return SearchEngineChoiceScreenConditions::kHasCustomSearchEngine;
  }

  // Force triggering the choice screen for testing the screen itself.
  // Deliberately checked after the conditions overriding the default search
  // engine with some custom one because they would put the choice screens in
  // some unstable state and they are rather easy to change if we want to
  // re-enable the triggering.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceSearchEngineChoiceScreen)) {
    return SearchEngineChoiceScreenConditions::kEligible;
  }

  if (IsSearchEngineChoiceCompleted(profile_prefs)) {
    return SearchEngineChoiceScreenConditions::kAlreadyCompleted;
  }

  return SearchEngineChoiceScreenConditions::kEligible;
}

int GetSearchEngineChoiceCountryId(PrefService* profile_prefs) {
  int command_line_country = country_codes::CountryStringToCountryID(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSearchEngineChoiceCountry));
  if (command_line_country != country_codes::kCountryIDUnknown) {
    return command_line_country;
  }

  bool force_eea_country =
      switches::kSearchEngineChoiceTriggerWithForceEeaCountry.Get();
  if (force_eea_country) {
    // `kSearchEngineChoiceTriggerWithForceEeaCountry` forces the search engine
    // choice country to Belgium.
    return country_codes::CountryStringToCountryID("BE");
  }

  return country_codes::GetCountryIDFromPrefs(profile_prefs);
}

bool IsEeaChoiceCountry(int country_id) {
  return GetEeaChoiceCountries().contains(country_id);
}

void RecordChoiceScreenProfileInitCondition(
    SearchEngineChoiceScreenConditions condition) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenProfileInitConditionsHistogram, condition);
}

void RecordChoiceScreenEvent(SearchEngineChoiceScreenEvents event) {
  base::UmaHistogramEnumeration(kSearchEngineChoiceScreenEventsHistogram,
                                event);
}

void RecordChoiceScreenDefaultSearchProviderType(SearchEngineType engine_type) {
  base::UmaHistogramEnumeration(
      kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram, engine_type,
      SEARCH_ENGINE_MAX);
}

void RecordChoiceMade(PrefService* profile_prefs,
                      ChoiceMadeLocation choice_location,
                      TemplateURLService* template_url_service) {
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return;
  }

  // Don't modify the pref if the user is not in the EEA region.
  if (!IsEeaChoiceCountry(GetSearchEngineChoiceCountryId(profile_prefs))) {
    return;
  }

  // Don't modify the prefs if they were already set.
  if (IsSearchEngineChoiceCompleted(*profile_prefs)) {
    return;
  }

  RecordChoiceScreenDefaultSearchProviderType(
      GetDefaultSearchEngineType(CHECK_DEREF(template_url_service)));
  profile_prefs->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  profile_prefs->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      version_info::GetVersionNumber());

  if (profile_prefs->HasPrefPath(prefs::kDefaultSearchProviderChoicePending)) {
    DVLOG(1) << "Choice made, removing profile tag.";
    profile_prefs->ClearPref(prefs::kDefaultSearchProviderChoicePending);
  }
}

void WipeSearchEngineChoicePrefs(PrefService& profile_prefs,
                                 WipeSearchEngineChoiceReason reason) {
  if (IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    base::UmaHistogramEnumeration(kSearchEngineChoiceWipeReasonHistogram,
                                  reason);
    profile_prefs.ClearPref(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp);
    profile_prefs.ClearPref(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion);
  }
}

#if !BUILDFLAG(IS_ANDROID)
std::u16string GetMarketingSnippetString(
    const TemplateURLData& template_url_data) {
  int snippet_resource_id =
      GetMarketingSnippetResourceId(template_url_data.keyword());

  return snippet_resource_id == -1
             ? l10n_util::GetStringFUTF16(
                   IDS_SEARCH_ENGINE_FALLBACK_MARKETING_SNIPPET,
                   template_url_data.short_name())
             : l10n_util::GetStringUTF16(snippet_resource_id);
}
#endif

void PreprocessPrefsForReprompt(PrefService& profile_prefs) {
  if (!IsChoiceScreenFlagEnabled(ChoicePromo::kAny)) {
    return;
  }

  // If existing prefs are missing or have a wrong format, force a reprompt.
  if (!profile_prefs.HasPrefPath(
          prefs::kDefaultSearchProviderChoiceScreenCompletionVersion)) {
    WipeSearchEngineChoicePrefs(
        profile_prefs, WipeSearchEngineChoiceReason::kMissingChoiceVersion);
    return;
  }

  base::Version choice_version(profile_prefs.GetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
  if (!IsValidVersionFormat(choice_version)) {
    WipeSearchEngineChoicePrefs(
        profile_prefs, WipeSearchEngineChoiceReason::kInvalidChoiceVersion);
    return;
  }

  // Check parameters from `switches::kSearchEngineChoiceTriggerRepromptParams`.
  absl::optional<base::Value::Dict> reprompt_params =
      base::JSONReader::ReadDict(
          switches::kSearchEngineChoiceTriggerRepromptParams.Get());
  if (!reprompt_params) {
    // No valid reprompt parameters.
    base::UmaHistogramEnumeration(kSearchEngineChoiceRepromptHistogram,
                                  RepromptResult::kInvalidDictionary);
    return;
  }

  const base::Version& current_version = version_info::GetVersion();
  int country_id = GetSearchEngineChoiceCountryId(&profile_prefs);
  const std::string wildcard_string("*");
  // Explicit country key takes precedence over the wildcard.
  for (const std::string& key :
       {country_codes::CountryIDToCountryString(country_id), wildcard_string}) {
    bool is_wildcard = key == wildcard_string;
    const std::string* reprompt_version_string =
        reprompt_params->FindString(key);
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

    if (choice_version >= reprompt_version) {
      // No need to reprompt, the choice is recent enough.
      LogSearchRepromptKeyHistograms(RepromptResult::kRecentChoice,
                                     is_wildcard);
      break;
    }

    // Wipe the choice to force a reprompt.
    LogSearchRepromptKeyHistograms(RepromptResult::kReprompt, is_wildcard);
    WipeSearchEngineChoicePrefs(profile_prefs,
                                WipeSearchEngineChoiceReason::kReprompt);
    return;
  }
}

}  // namespace search_engines
