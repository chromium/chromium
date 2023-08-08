// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/branding_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"

namespace safe_browsing::hash_realtime_utils {
// Used by tests so that more than just GOOGLE_CHROME_BRANDING bots are capable
// of running these tests.
bool kPretendHasGoogleChromeBranding = false;

namespace {
bool HasGoogleChromeBranding() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return kPretendHasGoogleChromeBranding;
#endif
}
}  // namespace

bool CanCheckUrl(const GURL& url,
                 network::mojom::RequestDestination request_destination) {
  // TODO(crbug.com/1444511): Add a histogram to see how many urls are filtered
  // by CanGetReputationOfUrl.
  return request_destination == network::mojom::RequestDestination::kDocument &&
         CanGetReputationOfUrl(url);
}

bool IsThreatTypeRelevant(const V5::ThreatType& threat_type) {
  switch (threat_type) {
    case V5::ThreatType::MALWARE:
    case V5::ThreatType::SOCIAL_ENGINEERING:
    case V5::ThreatType::UNWANTED_SOFTWARE:
#if !BUILDFLAG(IS_IOS)
    case V5::ThreatType::SUSPICIOUS:
#endif
    case V5::ThreatType::TRICK_TO_BILL:
      return true;
    default:
      // Using "default" because exhaustive switch statements are not
      // recommended for proto3 enums.
      return false;
  }
}
std::string GetHashPrefix(const std::string& full_hash) {
  DCHECK(full_hash.length() == kFullHashLength);
  return full_hash.substr(0, kHashPrefixLength);
}
bool IsHashRealTimeLookupEligibleInSession() {
  return HasGoogleChromeBranding() &&
         base::FeatureList::IsEnabled(kHashPrefixRealTimeLookups);
}
HashRealTimeSelection DetermineHashRealTimeSelection(
    bool is_off_the_record,
    PrefService* prefs,
    bool log_usage_histograms) {
  // All prefs used in this method must match the ones returned by
  // |GetHashRealTimeSelectionConfiguringPrefs| so that consumers listening for
  // changes can receive them correctly.
  struct Requirement {
    std::string failed_requirement_histogram_suffix;
    bool passes_requirement;
  } requirements[] = {
      {"IneligibleForSession",
       hash_realtime_utils::IsHashRealTimeLookupEligibleInSession()},
      {"OffTheRecord", !is_off_the_record},
      {"NotStandardProtection", safe_browsing::GetSafeBrowsingState(*prefs) ==
                                    SafeBrowsingState::STANDARD_PROTECTION},
      {"NotAllowedByPolicy",
       safe_browsing::AreHashPrefixRealTimeLookupsAllowedByPolicy(*prefs)}};
  bool can_do_lookup = true;
  for (const auto& requirement : requirements) {
    if (!requirement.passes_requirement) {
      can_do_lookup = false;
    }
    if (log_usage_histograms) {
      base::UmaHistogramBoolean(
          base::StrCat({"SafeBrowsing.HPRT.Ineligible.",
                        requirement.failed_requirement_histogram_suffix}),
          !requirement.passes_requirement);
    }
  }
  return can_do_lookup ?
#if BUILDFLAG(IS_ANDROID)
                       HashRealTimeSelection::kDatabaseManager
#else
                       HashRealTimeSelection::kHashRealTimeService
#endif
                       : HashRealTimeSelection::kNone;
}

std::vector<const char*> GetHashRealTimeSelectionConfiguringPrefs() {
  return {prefs::kSafeBrowsingEnabled, prefs::kSafeBrowsingEnhanced,
          prefs::kHashPrefixRealTimeChecksAllowedByPolicy};
}

GoogleChromeBrandingPretenderForTesting::
    GoogleChromeBrandingPretenderForTesting() {
  kPretendHasGoogleChromeBranding = true;
}
GoogleChromeBrandingPretenderForTesting::
    ~GoogleChromeBrandingPretenderForTesting() {
  StopApplyingBranding();
}
void GoogleChromeBrandingPretenderForTesting::StopApplyingBranding() {
  kPretendHasGoogleChromeBranding = false;
}

}  // namespace safe_browsing::hash_realtime_utils
