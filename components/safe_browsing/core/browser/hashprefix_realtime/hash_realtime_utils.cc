// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"

#include "base/check.h"
#include "build/branding_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing::hash_realtime_utils {
bool IsThreatTypeRelevant(const V5::ThreatType& threat_type) {
  switch (threat_type) {
    case V5::ThreatType::MALWARE:
    case V5::ThreatType::SOCIAL_ENGINEERING:
    case V5::ThreatType::UNWANTED_SOFTWARE:
    case V5::ThreatType::SUSPICIOUS:
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
  // TODO(crbug.com/1441654) [Also TODO(thefrog)]: Add GOOGLE_CHROME_BRANDING
  // check.
  return base::FeatureList::IsEnabled(kHashPrefixRealTimeLookups);
}
HashRealTimeSelection DetermineHashRealTimeSelection(bool is_off_the_record,
                                                     PrefService* prefs) {
  // All prefs used in this method must match the ones returned by
  // |GetHashRealTimeSelectionConfiguringPrefs| so that consumers listening for
  // changes can receive them correctly.
#if BUILDFLAG(IS_ANDROID)
  return HashRealTimeSelection::kNone;
#else
  bool can_do_lookup =
      hash_realtime_utils::IsHashRealTimeLookupEligibleInSession() &&
      !is_off_the_record &&
      safe_browsing::GetSafeBrowsingState(*prefs) ==
          SafeBrowsingState::STANDARD_PROTECTION &&
      safe_browsing::AreHashPrefixRealTimeLookupsAllowedByPolicy(*prefs);
  return can_do_lookup ? HashRealTimeSelection::kHashRealTimeService
                       : HashRealTimeSelection::kNone;
#endif
}

std::vector<const char*> GetHashRealTimeSelectionConfiguringPrefs() {
  return {prefs::kSafeBrowsingEnabled, prefs::kSafeBrowsingEnhanced,
          prefs::kHashPrefixRealTimeChecksAllowedByPolicy};
}

}  // namespace safe_browsing::hash_realtime_utils
