// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_service.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"

namespace regional_capabilities {
namespace {

constexpr char kUnknownCountryIdStored[] =
    "Search.ChoiceDebug.UnknownCountryIdStored";

// LINT.IfChange(UnknownCountryIdStored)
enum class UnknownCountryIdStored {
  kValidCountryId = 0,
  kDontClearInvalidCountry = 1,
  kClearedPref = 2,
  kMaxValue = kClearedPref,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/search/enums.xml:UnknownCountryIdStored)

}  // namespace

RegionalCapabilitiesService::RegionalCapabilitiesService(
    PrefService& profile_prefs,
    std::unique_ptr<Client> regional_capabilities_client)
    : profile_prefs_(profile_prefs),
      client_(std::move(regional_capabilities_client)) {
  CHECK(client_);
}

RegionalCapabilitiesService::~RegionalCapabilitiesService() = default;

int RegionalCapabilitiesService::GetCountryId() {
  std::optional<SearchEngineCountryOverride> country_override =
      GetSearchEngineCountryOverride();
  if (country_override.has_value()) {
    if (std::holds_alternative<int>(country_override.value())) {
      return std::get<int>(country_override.value());
    }
    return country_codes::kCountryIDUnknown;
  }

  if (!country_id_cache_.has_value()) {
    InitializeCountryIdCache();
  }

  return country_id_cache_.value();
}

void RegionalCapabilitiesService::InitializeCountryIdCache() {
  // TODO(crbug.com/328040066): Move `kCountryIDAtInstall` pref declaration in
  // this file / package.
  std::optional<int> country_id;

  // Check the validity of the initially persisted value, if present.
  if (profile_prefs_->HasPrefPath(country_codes::kCountryIDAtInstall)) {
    country_id = profile_prefs_->GetInteger(country_codes::kCountryIDAtInstall);
    if (country_id.value() != country_codes::kCountryIDUnknown) {
      base::UmaHistogramEnumeration(kUnknownCountryIdStored,
                                    UnknownCountryIdStored::kValidCountryId);
    } else {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
      if (base::FeatureList::IsEnabled(switches::kClearPrefForUnknownCountry)) {
        profile_prefs_->ClearPref(country_codes::kCountryIDAtInstall);
        country_id.reset();
        base::UmaHistogramEnumeration(kUnknownCountryIdStored,
                                      UnknownCountryIdStored::kClearedPref);
      } else
#endif
      {
        base::UmaHistogramEnumeration(
            kUnknownCountryIdStored,
            UnknownCountryIdStored::kDontClearInvalidCountry);
      }
    }
  }

  if (!country_id.has_value()) {
    client_->FetchCountryId(base::BindOnce(
        [](base::WeakPtr<RegionalCapabilitiesService> service, int country_id) {
          if (service && country_id != country_codes::kCountryIDUnknown) {
            service->profile_prefs_->SetInteger(
                country_codes::kCountryIDAtInstall, country_id);
          }
        },
        weak_ptr_factory_.GetWeakPtr()));
    if (profile_prefs_->HasPrefPath(country_codes::kCountryIDAtInstall)) {
      // The initialization above completed synchronously, return its outcome.
      country_id =
          profile_prefs_->GetInteger(country_codes::kCountryIDAtInstall);
    } else {
      // The initialization failed or did not complete synchronously. Use the
      // fallback value and don't persist it. If the fetch completes later, the
      // country will be picked up at the next startup.
      country_id = client_->GetFallbackCountryId();
    }
  }

  country_id_cache_ = country_id.value();
}

void RegionalCapabilitiesService::ClearCountryIdCacheForTesting() {
  CHECK_IS_TEST();
  country_id_cache_.reset();
}

}  // namespace regional_capabilities
