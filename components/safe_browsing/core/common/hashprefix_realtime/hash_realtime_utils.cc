// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "build/branding_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"

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

bool IsBackgroundLookupEligible(SafeBrowsingState safe_browsing_state,
                                bool are_background_lookups_allowed) {
  // Check feature flag after ESB check so that the Finch experiment
  // groups only include clients that can send sample pings.
  return safe_browsing_state == SafeBrowsingState::ENHANCED_PROTECTION &&
         are_background_lookups_allowed &&
         base::FeatureList::IsEnabled(kHashPrefixRealTimeLookupsSamplePing);
}

struct CommonRequirements {
  // Whether the user's browser session and country location are eligible for
  // hash-prefix real-time lookups.
  bool eligible_for_session_and_location;
  // Whether the user's Safe Browsing protection state is eligible.
  bool eligible_for_safe_browsing_state;
  // Whether hash-prefix real-time lookups are allowed by enterprise policy.
  bool allowed_by_policy;

  // Returns true if all common requirements are met.
  bool AreAllMet() const {
    return eligible_for_session_and_location &&
           eligible_for_safe_browsing_state && allowed_by_policy;
  }
};

CommonRequirements GetCommonRequirements(
    PrefService* prefs,
    const std::optional<std::string>& latest_country,
    bool eligible_for_safe_browsing_state) {
  // All prefs used in this method must match the ones returned by
  // |GetHashRealTimeSelectionConfiguringPrefs| so that consumers listening for
  // changes can receive them correctly.
  return {
      .eligible_for_session_and_location =
          hash_realtime_utils::IsHashRealTimeLookupEligibleInSessionAndLocation(
              latest_country),
      .eligible_for_safe_browsing_state = eligible_for_safe_browsing_state,
      .allowed_by_policy =
          safe_browsing::AreHashPrefixRealTimeLookupsAllowedByPolicy(*prefs),
  };
}
}  // namespace

bool CanCheckUrl(const GURL& url) {
  bool can_check_reputation = CanGetReputationOfUrl(url);
  base::UmaHistogramBoolean("SafeBrowsing.HPRT.CanGetReputationOfUrl",
                            can_check_reputation);
  return can_check_reputation;
}

bool IsHashDetailRelevant(const V5::FullHash::FullHashDetail& detail) {
  bool has_canary = false;
  bool has_frame_only = false;

  for (int attribute_value : detail.attributes()) {
    // LINT.IfChange(ThreatAttribute)
    switch (attribute_value) {
      case V5::ThreatAttribute::CANARY:
        has_canary = true;
        break;
      case V5::ThreatAttribute::FRAME_ONLY:
        has_frame_only = true;
        break;
      case V5::ThreatAttribute::THREAT_ATTRIBUTE_UNSPECIFIED:
        break;
        // LINT.ThenChange(//components/safe_browsing/core/common/proto/safebrowsingv5.proto:ThreatAttribute)
      default:
        // Using "default" because exhaustive switch statements are not
        // recommended for proto3 enums.
        break;
    }
  }

#if BUILDFLAG(IS_IOS)
    // iOS doesn't support CANARY threat attribute.
    if (has_canary) {
      return false;
    }
#endif

    // CANARY and FRAME_ONLY should not be set at the same time.
    if (has_canary && has_frame_only) {
      return false;
    }

    // CANARY should only be attached with SOCIAL_ENGINEERING,
    // ABUSIVE_EXPERIENCE_VIOLATION, BETTER_ADS_VIOLATION. Only
    // SOCIAL_ENGINEERING is relevant to hash real time checks (the others
    // are not frame URLs), so only checking SOCIAL_ENGINEERING here.
    // LINT.IfChange(ThreatType)
    switch (detail.threat_type()) {
      case V5::ThreatType::SOCIAL_ENGINEERING:
        // Social Engineering supports both warning (CANARY) and enforcement.
        return true;
      case V5::ThreatType::MALWARE:
      case V5::ThreatType::UNWANTED_SOFTWARE:
      case V5::ThreatType::TRICK_TO_BILL:
        // These types only support enforcement; CANARY is invalid.
        return !has_canary;
      case V5::ThreatType::THREAT_TYPE_UNSPECIFIED:
      case V5::ThreatType::POTENTIALLY_HARMFUL_APPLICATION:
      case V5::ThreatType::MALICIOUS_BINARY:
      case V5::ThreatType::SUBRESOURCE_FILTER:
      case V5::ThreatType::ABUSIVE_EXPERIENCE_VIOLATION:
      case V5::ThreatType::BETTER_ADS_VIOLATION:
      case V5::ThreatType::NOTIFICATION_ABUSE:
        // These types are not supported/relevant for HPRT queries.
        return false;
        // LINT.ThenChange(//components/safe_browsing/core/common/proto/safebrowsingv5.proto:ThreatType)
      default:
        // Using "default" because exhaustive switch statements are not
        // recommended for proto3 enums.
        return false;
    }
}

bool IsHashRealTimeLookupEligibleInSession() {
  return HasGoogleChromeBranding() &&
         base::FeatureList::IsEnabled(kHashPrefixRealTimeLookups);
}
bool IsHashRealTimeLookupEligibleInLocation(
    std::optional<std::string> latest_country) {
  return (
      !latest_country.has_value() ||
      !std::ranges::contains(GetExcludedCountries(), latest_country.value()));
}
bool IsHashRealTimeLookupEligibleInSessionAndLocation(
    std::optional<std::string> latest_country) {
  return IsHashRealTimeLookupEligibleInSession() &&
         IsHashRealTimeLookupEligibleInLocation(latest_country);
}
std::optional<std::string> GetCountryCode(
    variations::VariationsService* variations_service) {
  return variations_service ? variations_service->GetLatestCountry()
                            : std::optional<std::string>();
}
HashRealTimeSelection DetermineHashRealTimeSelection(
    bool is_off_the_record,
    PrefService* prefs,
    std::optional<std::string> latest_country,
    bool log_usage_histograms,
    bool are_background_lookups_allowed) {
  // All prefs used in this method must match the ones returned by
  // |GetHashRealTimeSelectionConfiguringPrefs| so that consumers listening for
  // changes can receive them correctly.
  SafeBrowsingState safe_browsing_state =
      safe_browsing::GetSafeBrowsingState(*prefs);
  // Enhanced Protection only uses HPRT for background sample pings in regular
  // mode, but is eligible for foreground HPRT lookups when off-the-record. We
  // intentionally do not check `kHashPrefixRealTimeLookupsOnIncognitoAndGuest`
  // here: the `"OffTheRecord"` requirement in `requirements` below gates all
  // off-the-record lookups on that feature flag, and deferring the check until
  // after `common_requirements.AreAllMet()` avoids activating the Finch study
  // when lookups are ineligible for other reasons.
  bool sb_state_eligible_for_foreground_lookup =
      safe_browsing_state == SafeBrowsingState::STANDARD_PROTECTION ||
      (safe_browsing_state == SafeBrowsingState::ENHANCED_PROTECTION &&
       is_off_the_record);
  bool only_background_lookup_eligible =
      !sb_state_eligible_for_foreground_lookup &&
      IsBackgroundLookupEligible(safe_browsing_state,
                                 are_background_lookups_allowed);
  CommonRequirements common_requirements =
      GetCommonRequirements(prefs, latest_country,
                            /*eligible_for_safe_browsing_state=*/
                            sb_state_eligible_for_foreground_lookup ||
                                only_background_lookup_eligible);

  struct Requirement {
    std::string failed_requirement_histogram_suffix;
    bool passes_requirement;
  } requirements[] = {
      {"IneligibleForSessionOrLocation",
       common_requirements.eligible_for_session_and_location},
      // The main required gating is just `!is_off_the_record ||
      // base::FeatureList::IsEnabled(
      // kHashPrefixRealTimeLookupsOnIncognitoAndGuest)`. However, also check
      // the other requirements are met to avoid activating the
      // kHashPrefixRealTimeLookupsOnIncognitoAndGuest Finch study when lookups
      // are ineligible for other reasons.
      {"OffTheRecord",
       !is_off_the_record ||
           (common_requirements.AreAllMet() &&
            base::FeatureList::IsEnabled(
                kHashPrefixRealTimeLookupsOnIncognitoAndGuest))},
      {"IneligibleForSafeBrowsingState",
       common_requirements.eligible_for_safe_browsing_state},
      {"NotAllowedByPolicy", common_requirements.allowed_by_policy}};
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
  if (log_usage_histograms) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.HPRT.Ineligible.NoGoogleChromeBranding",
        !HasGoogleChromeBranding());
    base::UmaHistogramBoolean(
        "SafeBrowsing.HPRT.Ineligible.FeatureOff",
        !base::FeatureList::IsEnabled(kHashPrefixRealTimeLookups));
    base::UmaHistogramBoolean(
        "SafeBrowsing.HPRT.Ineligible.IneligibleForLocation",
        !IsHashRealTimeLookupEligibleInLocation(latest_country));
  }
  return can_do_lookup
             ?
#if BUILDFLAG(IS_ANDROID)
             (only_background_lookup_eligible
                  ? HashRealTimeSelection::kDatabaseManagerBackgroundOnly
                  : HashRealTimeSelection::kDatabaseManager)
#else
             (only_background_lookup_eligible
                  ? HashRealTimeSelection::kHashRealTimeServiceBackgroundOnly
                  : HashRealTimeSelection::kHashRealTimeService)
#endif
             : HashRealTimeSelection::kNone;
}

bool AreOhttpKeyFetchesEnabled(PrefService* prefs,
                               std::optional<std::string> latest_country,
                               bool are_background_lookups_allowed) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  // All prefs used in this method must match the ones returned by
  // |GetHashRealTimeSelectionConfiguringPrefs| so that consumers listening for
  // changes can receive them correctly.
  SafeBrowsingState safe_browsing_state =
      safe_browsing::GetSafeBrowsingState(*prefs);
  bool eligible_for_safe_browsing_state =
      safe_browsing_state == SafeBrowsingState::STANDARD_PROTECTION ||
      (safe_browsing_state == SafeBrowsingState::ENHANCED_PROTECTION &&
       base::FeatureList::IsEnabled(
           kHashPrefixRealTimeLookupsOhttpKeyServiceForEsb)) ||
      IsBackgroundLookupEligible(safe_browsing_state,
                                 are_background_lookups_allowed);
  return GetCommonRequirements(prefs, latest_country,
                               eligible_for_safe_browsing_state)
      .AreAllMet();
#endif
}

HashRealTimeSelectionConfiguringPrefs
GetHashRealTimeSelectionConfiguringPrefs() {
  std::vector<const char*> profile_prefs = {
      prefs::kSafeBrowsingEnabled, prefs::kSafeBrowsingEnhanced,
      prefs::kHashPrefixRealTimeChecksAllowedByPolicy};
  // |kVariationsCountry| is used by |VariationsService::GetLatestCountry|.
  std::vector<const char*> local_state_prefs = {
      variations::prefs::kVariationsCountry};
  return HashRealTimeSelectionConfiguringPrefs(
      /*profile_prefs=*/profile_prefs,
      /*local_state_prefs=*/local_state_prefs);
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

HashRealTimeSelectionConfiguringPrefs::HashRealTimeSelectionConfiguringPrefs(
    std::vector<const char*>& profile_prefs,
    std::vector<const char*>& local_state_prefs)
    : profile_prefs(profile_prefs), local_state_prefs(local_state_prefs) {}

HashRealTimeSelectionConfiguringPrefs::
    ~HashRealTimeSelectionConfiguringPrefs() = default;

}  // namespace safe_browsing::hash_realtime_utils
