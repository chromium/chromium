// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_service.h"

#include <string>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/timezone.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace metrics::dwa {

// Set of countries in the European Economic Area. Used by
// RecordCoarseSystemInformation to set geo_designation fields in
// CoarseSystemInfo. These will need to be manually updated using
// "IsEuropeanEconomicArea" from go/source/user_preference_country.impl.gcl.
constexpr auto kEuropeanEconomicAreaCountries =
    base::MakeFixedFlatSet<std::string_view>({
        "at",  // Austria
        "be",  // Belgium
        "bg",  // Bulgaria
        "hr",  // Croatia
        "cy",  // Cyprus
        "cz",  // Czech Republic
        "dk",  // Denmark
        "ee",  // Estonia
        "fi",  // Finland
        "fr",  // France
        "de",  // Germany
        "gr",  // Greece
        "hu",  // Hungary
        "is",  // Iceland
        "ie",  // Ireland
        "it",  // Italy
        "lv",  // Latvia
        "li",  // Liechtenstein
        "lt",  // Lithuania
        "lu",  // Luxembourg
        "mt",  // Malta
        "nl",  // Netherlands
        "no",  // Norway
        "pl",  // Poland
        "pt",  // Portugal
        "ro",  // Romania
        "sk",  // Slovakia
        "si",  // Slovenia
        "es",  // Spain
        "se",  // Sweden
        "uk",  // United Kingdom
    });

// Number of seconds in a week or seven days. (604800 = 7 * 24 * 60 * 60)
const int kOneWeekInSeconds = 7 * 24 * 60 * 60;

DwaService::DwaService() = default;

DwaService::~DwaService() = default;

// static
void DwaService::RecordCoarseSystemInformation(
    MetricsServiceClient& client,
    const PrefService& local_state,
    ::dwa::CoarseSystemInfo* coarse_system_info) {
  switch (client.GetChannel()) {
    case SystemProfileProto::CHANNEL_STABLE:
      coarse_system_info->set_channel(::dwa::CoarseSystemInfo::CHANNEL_STABLE);
      break;
    case SystemProfileProto::CHANNEL_CANARY:
    case SystemProfileProto::CHANNEL_DEV:
    case SystemProfileProto::CHANNEL_BETA:
      coarse_system_info->set_channel(
          ::dwa::CoarseSystemInfo::CHANNEL_NOT_STABLE);
      break;
    case SystemProfileProto::CHANNEL_UNKNOWN:
      coarse_system_info->set_channel(::dwa::CoarseSystemInfo::CHANNEL_INVALID);
      break;
  }

#if BUILDFLAG(IS_WIN)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_WINDOWS);
#elif BUILDFLAG(IS_MAC)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_MACOS);
#elif BUILDFLAG(IS_LINUX)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_LINUX);
#elif BUILDFLAG(IS_ANDROID)
  // TODO(b/366276323): Populate set_platform using more granular
  // PLATFORM_ANDROID enum.
  coarse_system_info->set_platform(
      ::dwa::CoarseSystemInfo::PLATFORM_ANDROID_BROWSER_APP);
#elif BUILDFLAG(IS_IOS)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_IOS);
#elif BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_CHROMEOS);
#else
  coarse_system_info->set_platform(::dwa::CoarseSystemInfo::PLATFORM_OTHER);
#endif

  std::string country =
      base::ToLowerASCII(base::CountryCodeForCurrentTimezone());
  if (country == "") {
    coarse_system_info->set_geo_designation(
        ::dwa::CoarseSystemInfo::GEO_DESIGNATION_INVALID);
  } else if (kEuropeanEconomicAreaCountries.contains(country)) {
    coarse_system_info->set_geo_designation(
        ::dwa::CoarseSystemInfo::GEO_DESIGNATION_EEA);
  } else {
    // GEO_DESIGNATION_ROW is the geo designation for "rest of the world".
    coarse_system_info->set_geo_designation(
        ::dwa::CoarseSystemInfo::GEO_DESIGNATION_ROW);
  }

  int64_t seconds_since_install =
      MetricsLog::GetCurrentTime() - local_state.GetInt64(prefs::kInstallDate);
  coarse_system_info->set_client_age(
      seconds_since_install < kOneWeekInSeconds
          ? ::dwa::CoarseSystemInfo::CLIENT_AGE_RECENT
          : ::dwa::CoarseSystemInfo::CLIENT_AGE_NOT_RECENT);

  // GetVersion() returns base::Version, which represents a dotted version
  // number, like "1.2.3.4". We %16 in milestone_prefix_trimmed because it is
  // required by the DWA proto in
  // //third_party/metrics_proto/dwa/deidentified_web_analytics.proto.
  int milestone = version_info::GetVersion().components()[0];
  coarse_system_info->set_milestone_prefix_trimmed(milestone % 16);

  coarse_system_info->set_is_ukm_enabled(client.IsUkmAllowedForAllProfiles());
}

}  // namespace metrics::dwa
