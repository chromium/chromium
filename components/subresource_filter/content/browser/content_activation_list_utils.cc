// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_activation_list_utils.h"

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"

namespace subresource_filter {

namespace {

ActivationList GetSubresourceFilterMatch(
    const safe_browsing::ThreatMetadata& threat_type_metadata,
    bool* warning) {
  auto better_ads_it = threat_type_metadata.subresource_filter_match.find(
      safe_browsing::SubresourceFilterType::BETTER_ADS);
  bool has_better_ads =
      better_ads_it != threat_type_metadata.subresource_filter_match.end();
  auto abusive_it = threat_type_metadata.subresource_filter_match.find(
      safe_browsing::SubresourceFilterType::ABUSIVE);
  bool has_abusive =
      abusive_it != threat_type_metadata.subresource_filter_match.end() &&
      base::FeatureList::IsEnabled(kFilterAdsOnAbusiveSites);

  // If both |BETTER_ADS| and |ABUSIVE| are in the map, the one with |ENFORCE|
  // level is chosen. If it's a tie, we arbitrarily give |BETTER_ADS| a higher
  // priority over |ABUSIVE|.
  if (has_better_ads && has_abusive) {
    if (better_ads_it->second == safe_browsing::SubresourceFilterLevel::ENFORCE)
      return ActivationList::BETTER_ADS;
    if (abusive_it->second == safe_browsing::SubresourceFilterLevel::ENFORCE)
      return ActivationList::ABUSIVE;
    *warning = true;
    return ActivationList::BETTER_ADS;
  }
  if (has_better_ads) {
    *warning =
        better_ads_it->second == safe_browsing::SubresourceFilterLevel::WARN;
    return ActivationList::BETTER_ADS;
  }
  if (has_abusive) {
    *warning =
        abusive_it->second == safe_browsing::SubresourceFilterLevel::WARN;
    return ActivationList::ABUSIVE;
  }

  // Keep a generic subresource_filter list without warning implemented, for
  // subresource filter matches with no metadata.
  if (threat_type_metadata.subresource_filter_match.empty())
    return ActivationList::SUBRESOURCE_FILTER;

  return ActivationList::NONE;
}

}  // namespace

ActivationList GetListForThreatTypeAndMetadata(
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& threat_type_metadata,
    bool* warning) {
  CHECK(warning, base::NotFatalUntil::M129);
  bool is_phishing_interstitial =
      (threat_type == safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  bool is_soc_engineering_ads_interstitial =
      threat_type_metadata.threat_pattern_type ==
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS;
  if (is_phishing_interstitial) {
    if (is_soc_engineering_ads_interstitial) {
      return ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL;
    }
    return ActivationList::PHISHING_INTERSTITIAL;
  } else if (threat_type ==
             safe_browsing::SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
    return GetSubresourceFilterMatch(threat_type_metadata, warning);
  }
  return ActivationList::NONE;
}

}  // namespace subresource_filter
