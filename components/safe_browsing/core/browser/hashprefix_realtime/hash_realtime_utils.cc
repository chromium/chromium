// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"

#include "base/check.h"
#include "base/containers/contains.h"
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
  bool can_check_reputation = CanGetReputationOfUrl(url);
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.CanGetReputationOfUrl",
                            can_check_reputation);
  return request_destination == network::mojom::RequestDestination::kDocument &&
         can_check_reputation;
}

bool IsHashDetailRelevant(const V5::FullHash::FullHashDetail& detail) {
  if (base::Contains(detail.attributes(), V5::ThreatAttribute::CANARY)) {
#if BUILDFLAG(IS_IOS)
    // iOS doesn't support CANARY threat attribute.
    return false;
#else
    if (detail.threat_type() != V5::ThreatType::SOCIAL_ENGINEERING) {
      // CANARY should only be attached with SOCIAL_ENGINEERING,
      // ABUSIVE_EXPERIENCE_VIOLATION, BETTER_ADS_VIOLATION or API_ABUSE. Only
      // SOCIAL_ENGINEERING is relevant to hash real time checks (the others
      // are not frame URLs), so only checking SOCIAL_ENGINEERING here.
      return false;
    }
    if (base::Contains(detail.attributes(), V5::ThreatAttribute::FRAME_ONLY)) {
      // CANARY and FRAME_ONLY should not be set at the same time.
      return false;
    }
#endif
  }

  switch (detail.threat_type()) {
    case V5::ThreatType::MALWARE:
    case V5::ThreatType::SOCIAL_ENGINEERING:
    case V5::ThreatType::UNWANTED_SOFTWARE:
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
