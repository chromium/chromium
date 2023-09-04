// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/signin/public/base/signin_switches.h"

namespace search_engines {
namespace {

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

const base::flat_set<int> GetEeaChoiceCountries() {
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

}  // namespace

bool ShouldShowChoiceScreen(const policy::PolicyService& policy_service,
                            const ProfileProperties& profile_properties) {
  if (!base::FeatureList::IsEnabled(switches::kSearchEngineChoice)) {
    return false;
  }

  PrefService& prefs = CHECK_DEREF(profile_properties.pref_service.get());

  // The timestamp indicates that the user has already made a search engine
  // choice in the choice screen.
  if (prefs.GetInt64(
          prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp)) {
    return false;
  }

  if (!IsEeaChoiceCountry(GetSearchEngineChoiceCountryId(prefs))) {
    return false;
  }

  return profile_properties.is_regular_profile &&
         IsSearchEngineChoiceScreenAllowedByPolicy(policy_service);
}

int GetSearchEngineChoiceCountryId(PrefService& profile_prefs) {
  int command_line_country = country_codes::CountryStringToCountryID(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSearchEngineChoiceCountry));
  if (command_line_country != country_codes::kCountryIDUnknown) {
    return command_line_country;
  }

  return country_codes::GetCountryIDFromPrefs(&profile_prefs);
}

bool IsEeaChoiceCountry(int country_id) {
  return GetEeaChoiceCountries().contains(country_id);
}
}  // namespace search_engines
