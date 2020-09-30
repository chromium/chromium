// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UTIL_H_
#define COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UTIL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

class GURL;

namespace lookalikes {
extern const char kHistogramName[];

// Register applicable preferences with the provided registry.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
}

using LookalikeTargetAllowlistChecker =
    base::RepeatingCallback<bool(const std::string&)>;

// Used for |GetTargetEmbeddingType| return value. It shows if the target
// embedding triggers on the input domain, and if it does, what type of warning
// should be shown to the user.
enum class TargetEmbeddingType {
  kNone = 0,
  kInterstitial = 1,
  kSafetyTip = 2,
};

// Used for UKM. There is only a single LookalikeUrlMatchType per navigation.
enum class LookalikeUrlMatchType {
  kNone = 0,
  // DEPRECATED: Use kSkeletonMatchTop500 or kSkeletonMatchTop5k.
  // kTopSite = 1,
  kSiteEngagement = 2,
  kEditDistance = 3,
  kEditDistanceSiteEngagement = 4,
  kTargetEmbedding = 5,
  kSkeletonMatchTop500 = 6,
  kSkeletonMatchTop5k = 7,
  kTargetEmbeddingForSafetyTips = 8,

  // The domain name failed IDN spoof checks but didn't match a safe hostname.
  // As a result, there is no URL to suggest to the user in the form of "Did
  // you mean <url>?".
  kFailedSpoofChecks = 9,

  // Append new items to the end of the list above; do not modify or replace
  // existing values. Comment out obsolete items.
  kMaxValue = kFailedSpoofChecks,
};

// Used for UKM. There is only a single LookalikeUrlBlockingPageUserAction per
// navigation.
enum class LookalikeUrlBlockingPageUserAction {
  kInterstitialNotShown = 0,
  kClickThrough = 1,
  kAcceptSuggestion = 2,
  kCloseOrBack = 3,

  // Append new items to the end of the list above; do not modify or replace
  // existing values. Comment out obsolete items.
  kMaxValue = kCloseOrBack,
};

// Used for metrics. Multiple events can occur per navigation.
enum class NavigationSuggestionEvent {
  kNone = 0,
  // Interstitial results recorded using security_interstitials::MetricsHelper
  // kInfobarShown = 1,
  // kLinkClicked = 2,
  // DEPRECATED: Use kMatchSkeletonTop500 or kMatchSkeletonTop5k.
  // kMatchTopSite = 3,
  kMatchSiteEngagement = 4,
  kMatchEditDistance = 5,
  kMatchEditDistanceSiteEngagement = 6,
  kMatchTargetEmbedding = 7,
  kMatchSkeletonTop500 = 8,
  kMatchSkeletonTop5k = 9,
  kMatchTargetEmbeddingForSafetyTips = 10,
  kFailedSpoofChecks = 11,

  // Append new items to the end of the list above; do not modify or
  // replace existing values. Comment out obsolete items.
  kMaxValue = kFailedSpoofChecks,
};

struct DomainInfo {
  // The full ASCII hostname, used in detecting target embedding. For
  // "https://www.google.com/mail" this will be "www.google.com".
  const std::string hostname;
  // eTLD+1, used for skeleton and edit distance comparison. Must be ASCII.
  // Empty for non-unique domains, localhost or sites whose eTLD+1 is empty.
  const std::string domain_and_registry;
  // eTLD+1 without the registry part, and with a trailing period. For
  // "www.google.com", this will be "google.". Used for edit distance
  // comparisons. Empty for non-unique domains, localhost or sites whose eTLD+1
  // is empty.
  const std::string domain_without_registry;

  // Result of IDN conversion of domain_and_registry field.
  const url_formatter::IDNConversionResult idn_result;
  // Skeletons of domain_and_registry field.
  const url_formatter::Skeletons skeletons;

  DomainInfo(const std::string& arg_hostname,
             const std::string& arg_domain_and_registry,
             const std::string& arg_domain_without_registry,
             const url_formatter::IDNConversionResult& arg_idn_result,
             const url_formatter::Skeletons& arg_skeletons);
  ~DomainInfo();
  DomainInfo(const DomainInfo& other);
};

// Returns a DomainInfo instance computed from |hostname|. Will return empty
// fields for non-unique hostnames (e.g. site.test), localhost or sites whose
// eTLD+1 is empty.
DomainInfo GetDomainInfo(const std::string& hostname);

// Convenience function for returning GetDomainInfo(url.host()).
DomainInfo GetDomainInfo(const GURL& url);

// Returns true if the Levenshtein distance between |str1| and |str2| is at most
// one. This has O(max(n,m)) complexity as opposed to O(n*m) of the usual edit
// distance computation.
bool IsEditDistanceAtMostOne(const base::string16& str1,
                             const base::string16& str2);

// Returns whether |navigated_domain| and |matched_domain| are likely to be edit
// distance false positives, and thus the user should *not* be warned.
//
// Assumes |navigated_domain| and |matched_domain| are edit distance matches.
bool IsLikelyEditDistanceFalsePositive(const DomainInfo& navigated_domain,
                                       const DomainInfo& matched_domain);

// Returns true if the domain given by |domain_info| is a top domain.
bool IsTopDomain(const DomainInfo& domain_info);

// Returns eTLD+1 of |hostname|. This excludes private registries, and returns
// "blogspot.com" for "test.blogspot.com" (blogspot.com is listed as a private
// registry). We do this to be consistent with url_formatter's top domain list
// which doesn't have a notion of private registries.
std::string GetETLDPlusOne(const std::string& hostname);

// Returns true if a lookalike interstitial should be shown.
bool ShouldBlockLookalikeUrlNavigation(LookalikeUrlMatchType match_type);

// Returns true if a domain is visually similar to the hostname of |url|. The
// matching domain can be a top domain or an engaged site. Similarity
// check is made using both visual skeleton and edit distance comparison.  If
// this returns true, match details will be written into |matched_domain|.
// Pointer arguments can't be nullptr.
bool GetMatchingDomain(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type);

void RecordUMAFromMatchType(LookalikeUrlMatchType match_type);

// Checks to see if a URL is a target embedding lookalike. This function sets
// |safe_hostname| to the url of the embedded target domain.
// At the moment we consider the following cases as Target Embedding:
// example-google.com-site.com, example.google.com-site.com,
// example-google-info-site.com, example.google.com.site.com,
// example-googlé.com-site.com where the embedded target is google.com. We
// detect embeddings of top 500 domains and engaged domains. However, to reduce
// false positives, we do not protect domains that are shorter than 7 characters
// long (e.g. com.ru).
// This function checks possible targets against |in_target_allowlist| to skip
// permitted embeddings.
// If no target embedding is found, the return value will be set to |kNonw|.
// When the target is embedded with another TLD instead of its actual TLD, it
// should trigger a Safety Tip when the embedded TLD is a ccTLD. In this
// situation, return value will be |kSafetyTip|. All the other triggers will
// result in a |kInterstitial| return value.
TargetEmbeddingType GetTargetEmbeddingType(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    std::string* safe_hostname);

// Returns true if a navigation to an IDN should be blocked.
bool ShouldBlockBySpoofCheckResult(const DomainInfo& navigated_domain);

// Checks whether the given url is allowlisted by enterprise policy, and
// thus no warnings should be shown on that host.
bool IsAllowedByEnterprisePolicy(const PrefService* pref_service,
                                 const GURL& url);

// Add the given hosts to the allowlist policy setting.
void SetEnterpriseAllowlistForTesting(PrefService* pref_service,
                                      const std::vector<std::string>& hosts);

#endif  // COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UTIL_H_
