// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UTIL_H_
#define COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UTIL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "components/lookalikes/core/safety_tips.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "components/version_info/channel.h"
#include "url/gurl.h"

namespace lookalikes {

// Name of the histogram recorded by the interstitial for lookalike match types.
extern const char kInterstitialHistogramName[];

// Register applicable preferences with the provided registry.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns the console message to be shown in devtools when a URL is flagged by
// a lookalike heuristic. If is_new_heuristic is true, the message is for a new
// heuristic that's not fully launched and it has an extra line about future
// behavior of Chrome.
std::string GetConsoleMessage(const GURL& lookalike_url, bool is_new_heuristic);

// Used for |GetTargetEmbeddingType| return value. It shows if the target
// embedding triggers on the input domain, and if it does, what type of warning
// should be shown to the user.
enum class TargetEmbeddingType {
  kNone = 0,
  kInterstitial = 1,
  kSafetyTip = 2,
};

// Used for |GetComboSquattingType| return value.
// It shows if the brand name in the flagged domain
// comes from the hard-coded brand names or from site engagements.
enum class ComboSquattingType {
  kNone = 0,
  kHardCoded = 1,
  kSiteEngagement = 2,
};

// Used for UKM. There is only a single LookalikeUrlMatchType per navigation.
enum class LookalikeUrlMatchType {
  kNone = 0,
  // DEPRECATED: Use kSkeletonMatchTop500 or kSkeletonMatchTop5k.
  // kTopSite = 1,
  kSkeletonMatchSiteEngagement = 2,
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

  kCharacterSwapSiteEngagement = 10,
  kCharacterSwapTop500 = 11,

  // Combo Squatting uses manually
  // curated lists of hard-coded keywords (kPopularKeywordsforCSQ in
  // lookalike_url_util.cc) and both manually curated hard-coded brand names
  // (kBrandNamesforCSQ in lookalike_url_util.cc) and brand names from
  // SiteEngagement to flag domains.
  kComboSquatting = 12,
  kComboSquattingSiteEngagement = 13,

  // Append new items to the end of the list above; do not modify or replace
  // existing values. Comment out obsolete items.
  kMaxValue = kComboSquattingSiteEngagement,
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
  kMatchCharacterSwapSiteEngagement = 12,
  kMatchCharacterSwapTop500 = 13,
  kComboSquatting = 14,
  kComboSquattingSiteEngagement = 15,

  // Append new items to the end of the list above; do not modify or
  // replace existing values. Comment out obsolete items.
  kMaxValue = kComboSquattingSiteEngagement,
};

struct TopBucketDomainsParams {
  // Skeletons of top bucket domains. This is the top 500 or 1000 most popular
  // domains (though, there can be fewer than 500 or 1000 skeletons in this
  // array).
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const char* const* edit_distance_skeletons;
  // Number of skeletons in `edit_distance_skeletons`.
  size_t num_edit_distance_skeletons;
};

struct ComboSquattingParams {
  // An array of brand names (such as "google", "youtube") and their skeletons
  // (in pairs). The first item in each pair is the brand name and the second
  // item is its skeleton. Brand names should be usable in domain names (i.e.
  // lower case, no punctuation except for - etc.)
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const std::pair<const char*, const char*>* brand_names;
  // Number of brand names in combo_squatting_brand_names.
  size_t num_brand_names;

  // List of popular keywords such as "login", "online".
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const char* const* popular_keywords;
  // Number of popular keywords in combo_squatting_keywords.
  size_t num_popular_keywords;
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
  // Skeletons of domain_without_registry field.
  const url_formatter::Skeletons domain_without_registry_skeletons;

  DomainInfo(
      const std::string& arg_hostname,
      const std::string& arg_domain_and_registry,
      const std::string& arg_domain_without_registry,
      const url_formatter::IDNConversionResult& arg_idn_result,
      const url_formatter::Skeletons& arg_skeletons,
      const url_formatter::Skeletons& arg_domain_without_registry_skeletons);
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
bool IsEditDistanceAtMostOne(const std::u16string& str1,
                             const std::u16string& str2);

// Returns whether |navigated_domain| and |matched_domain| are likely to be edit
// distance false positives, and thus the user should *not* be warned.
//
// Assumes |navigated_domain| and |matched_domain| are edit distance of 1 apart.
bool IsLikelyEditDistanceFalsePositive(const DomainInfo& navigated_domain,
                                       const DomainInfo& matched_domain);

// Returns whether |navigated_domain| and |matched_domain| are likely to be
// character swap false positives, and thus the user should *not* be warned.
//
// Assumes |navigated_domain| and |matched_domain| are within 1 character swap.
bool IsLikelyCharacterSwapFalsePositive(const DomainInfo& navigated_domain,
                                        const DomainInfo& matched_domain);

// Returns true if the domain given by |domain_info| is a top domain.
bool IsTopDomain(const DomainInfo& domain_info);

// Returns eTLD+1 of |hostname|. This excludes private registries, and returns
// "blogspot.com" for "test.blogspot.com" (blogspot.com is listed as a private
// registry). We do this to be consistent with url_formatter's top domain list
// which doesn't have a notion of private registries.
std::string GetETLDPlusOne(const std::string& hostname);

// Records an interstitial histogram entry for the given match type.
void RecordUMAFromMatchType(LookalikeUrlMatchType match_type);

using LookalikeTargetAllowlistChecker =
    base::RepeatingCallback<bool(const std::string&)>;

// Returns true if a domain is visually similar to the hostname of |url|. The
// matching domain can be a top domain or an engaged site. Similarity
// check is made using both visual skeleton and edit distance comparison.  If
// this returns true, match details will be written into |matched_domain|.
// Pointer arguments can't be nullptr.
bool GetMatchingDomain(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type);

// Checks to see if a URL is a target embedding lookalike. This function sets
// |safe_hostname| to the url of the embedded target domain. See the unit tests
// for what qualifies as target embedding.
TargetEmbeddingType GetTargetEmbeddingType(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    std::string* safe_hostname);

// Same as GetTargetEmbeddingType, but explicitly state whether or not a safety
// tip is permitted via |safety_tips_allowed|. Safety tips are presently only
// used for tail embedding (e.g. "evil-google.com"). This function may return
// kSafetyTip preferentially to kInterstitial -- call with !safety_tips_allowed
// if you're interested in determining if there's *also* an interstitial.
TargetEmbeddingType SearchForEmbeddings(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    bool safety_tips_allowed,
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

// Returns true if |str1| and |str2| are identical except that two adjacent
// characters are swapped. E.g. example.com vs exapmle.com.
bool HasOneCharacterSwap(const std::u16string& str1,
                         const std::u16string& str2);

// Sets information about top bucket domains for testing.
void SetTopBucketDomainsParamsForTesting(const TopBucketDomainsParams& params);
// Resets information about top bucket domains for testing.
void ResetTopBucketDomainsParamsForTesting();

// Returns true if the launch configuration provided by the component updater
// enables `heuristic` for the given `etld_plus_one`.
bool IsHeuristicEnabledForHostname(
    const reputation::SafetyTipsConfig* config_proto,
    reputation::HeuristicLaunchConfig::Heuristic heuristic,
    const std::string& lookalike_etld_plus_one,
    version_info::Channel channel);

// Set brand names and keywords for testing Combo Squatting heuristic.
void SetComboSquattingParamsForTesting(const ComboSquattingParams& params);

// Reset brand names and keywords after testing Combo Squatting heuristic.
void ResetComboSquattingParamsForTesting();

// Check if |navigated_domain| is Combo Squatting lookalike.
// It gets |engaged_sites| to use its brand names in addition to hard coded
// brand names. The function sets |matched_domain| to suggest to the user
// instead of the Combo Squatting domain.
ComboSquattingType GetComboSquattingType(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    std::string* matched_domain);

// Returns true if `etld_plus_one` has a TLD that's considered safe for
// lookalike checks, such as government sites.
bool IsSafeTLD(const std::string& hostname);

// The action to take for a given lookalike match.
enum class LookalikeActionType {
  // No action.
  kNone,
  // Only record metrics, don't show any UI warnings.
  kRecordMetrics,
  // Show a safety tip.
  kShowSafetyTip,
  // Show an interstitial.
  kShowInterstitial,
};

// Returns the action to take for the given `etld_plus_one` and lookalike
// `match_type`. Uses `config` to check whether the heuristic UI is enabled
// via gradual rollout.
LookalikeActionType GetActionForMatchType(
    const reputation::SafetyTipsConfig* config,
    version_info::Channel channel,
    const std::string& etld_plus_one,
    LookalikeUrlMatchType match_type);

// Returns the suggested URL for the given parameters. Returns an https URL for
// top domain matches because it's more likely for top sites to support https.
GURL GetSuggestedURL(LookalikeUrlMatchType match_type,
                     const GURL& navigated_url,
                     const std::string& matched_hostname);

}  // namespace lookalikes

#endif  // COMPONENTS_LOOKALIKES_CORE_LOOKALIKE_URL_UTIL_H_
