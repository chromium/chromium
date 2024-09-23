// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/lookalikes/core/lookalike_url_util.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/hash/sha1.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/lookalikes/core/safety_tips_config.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/url_formatter/spoof_checks/common_words/common_words_util.h"
#include "components/url_formatter/spoof_checks/top_domains/top_bucket_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

using lookalikes::ComboSquattingParams;
using lookalikes::DomainInfo;
using lookalikes::GetDomainInfo;
using lookalikes::HasOneCharacterSwap;
using lookalikes::IsEditDistanceAtMostOne;
using lookalikes::LookalikeTargetAllowlistChecker;
using lookalikes::LookalikeUrlMatchType;
using lookalikes::NavigationSuggestionEvent;
using lookalikes::TopBucketDomainsParams;

namespace {

// Digits. Used for trimming domains in Edit Distance heuristic matches. Domains
// that only differ by trailing digits (e.g. a1.tld and a2.tld) are ignored.
const char kDigitChars[] = "0123456789";

// Minimum length of e2LD protected against target embedding. For example,
// foo.bar.baz.com-evil.com embeds foo.bar.baz.com, but we don't flag it since
// "baz" is shorter than kMinTargetE2LDLength.
const size_t kMinE2LDLengthForTargetEmbedding = 4;

// We might not protect a domain whose e2LD is a common word in target embedding
// based on the TLD that is paired with it. This list supplements words from
// url_formatter::common_words::IsCommonWord().
const char* kLocalAdditionalCommonWords[] = {"asahi", "hoteles", "jharkhand",
                                             "nifty"};

// These domains are plausible lookalike targets, but they also use common words
// in their names. Selectively prevent flagging embeddings where the embedder
// ends in "-DOMAIN.TLD", since these tend to have higher false positive rates.
const char* kDomainsPermittedInEndEmbeddings[] = {"office.com", "medium.com",
                                                  "orange.fr"};

// What separators can be used to separate tokens in target embedding spoofs?
// e.g. www-google.com.example.com uses "-" (www-google) and "." (google.com).
const char kTargetEmbeddingSeparators[] = "-.";

// A small subset of private registries on the PSL that act like public
// registries AND are a common source of false positives in lookalike checks. We
// treat them as public for the purposes of lookalike checks.
const char* kPrivateRegistriesTreatedAsPublic[] = {"com.de", "com.se"};

TopBucketDomainsParams* GetTopDomainParams() {
  static TopBucketDomainsParams params{
      top_bucket_domains::kTopBucketEditDistanceSkeletons,
      top_bucket_domains::kNumTopBucketEditDistanceSkeletons};
  return &params;
}

// Minimum length of the eTLD+1 without registry needed to show the punycode
// interstitial. IDN whose eTLD+1 without registry is shorter than this are
// still displayed in punycode, but don't show an interstitial.
const size_t kMinimumE2LDLengthToShowPunycodeInterstitial = 2;

// Default launch percentage of a new heuristic on Canary/Dev and Beta. These
// are used if there is a launch config for the heuristic in the proto.
const int kDefaultLaunchPercentageOnCanaryDev = 90;
const int kDefaultLaunchPercentageOnBeta = 50;

// Define skeletons of brand names and popular keywords for using in Combo
// Squatting heuristic. These lists are manually curated using Chrome metrics.
// We will check combinations of brand names and popular keywords.
// e. g. google-login.com or youtubesecure.com.
// For every brand name, brand_name[.]com should be checked to be valid. If
// no matched domain is found in top domains, brand_name[.]com will be
// suggested to the user for navigation.
// If brand_name[.]com is not valid for any brand name, each brand name should
// be mapped to a valid url manually and the data structure of
//  ForCSQ should be changed accordingly.
// In each element of kBrandNamesForCSQ, first string is an original brand name
// and second string is its skeleton.
// If you are adding a brand name here, you can generate its skeleton using the
// format_url binary (components/url_formatter/tools/format_url.cc)
// TODO(crbug.com/40855941): Generate skeletons of hard coded brand names in
// Chrome initialization and remove manual adding of skeletons to this list.
constexpr std::pair<const char*, const char*> kBrandNamesForCSQ[] = {
    {"adobe", "adobe"},
    {"airbnb", "airbnb"},
    {"alibaba", "alibaba"},
    {"aliexpress", "aliexpress"},
    {"amazon", "arnazon"},
    {"baidu", "baidu"},
    {"bestbuy", "bestbuy"},
    {"blogspot", "blogspot"},
    {"costco", "costco"},
    {"craigslist", "craigslist"},
    {"dropbox", "dropbox"},
    {"expedia", "expedia"},
    {"facebook", "facebook"},
    {"fedex", "fedex"},
    {"flickr", "flickr"},
    {"github", "github"},
    {"glassdoor", "glassdoor"},
    {"gofundme", "gofundrne"},
    {"google", "google"},
    {"homedepot", "hornedepot"},
    {"icloud", "icloud"},
    {"indeed", "indeed"},
    {"instagram", "instagrarn"},
    {"intuit", "intuit"},
    {"microsoft", "rnicrosoft"},
    {"nbcnews", "nbcnews"},
    {"netflix", "netflix"},
    {"norton", "norton"},
    {"nytimes", "nytirnes"},
    {"office365", "office365"},
    {"paypal", "paypal"},
    {"pinterest", "pinterest"},
    {"playstation", "playstation"},
    {"quora", "quora"},
    {"reddit", "reddit"},
    {"reuters", "reuters"},
    {"samsung", "sarnsung"},
    {"spotify", "spotify"},
    {"stackexchange", "stackexchange"},
    {"stackoverflow", "stackoverflow"},
    {"trello", "trello"},
    {"twitch", "twitch"},
    {"twitter", "twitter"},
    {"uderny", "udemy"},
    {"wikipedia", "wikipedia"},
    {"wordpress", "wordpress"},
    {"xfinity", "xfinity"},
    {"yahoo", "yahoo"},
    {"youtube", "youtube"},
    {"zillow", "zillow"}};

// Each element in kSkeletonsOfPopularKeywordsForCSQ is a skeleton of a popular
// keyword. In contrast to kBrandNamesForCSQ, the original keywords are not
// included. Because in kBrandNamesForCSQ, original brand names are used to
// generate the matched domain, and original keywords are not needed for that
// process.
// If you are adding a keyword here, you can generate its skeleton
// using the format_url binary (components/url_formatter/tools/format_url.cc)
const char* kSkeletonsOfPopularKeywordsForCSQ[] = {
    // Security
    "account",  "activate", "adrnin",   "coin",   "crypto",  "login", "logout",
    "password", "secure",   "security", "signin", "signout", "wallet"};

// Minimum length of brand to be checked for Combo Squatting.
const size_t kMinBrandNameLengthForComboSquatting = 4;

ComboSquattingParams* GetComboSquattingParams() {
  static ComboSquattingParams params{
      kBrandNamesForCSQ, std::size(kBrandNamesForCSQ),
      kSkeletonsOfPopularKeywordsForCSQ,
      std::size(kSkeletonsOfPopularKeywordsForCSQ)};
  return &params;
}

bool SkeletonsMatch(const url_formatter::Skeletons& skeletons1,
                    const url_formatter::Skeletons& skeletons2) {
  DCHECK(!skeletons1.empty());
  DCHECK(!skeletons2.empty());
  for (const std::string& skeleton1 : skeletons1) {
    if (base::Contains(skeletons2, skeleton1)) {
      return true;
    }
  }
  return false;
}

// Returns a site that the user has used before that the eTLD+1 in
// |domain_and_registry| may be attempting to spoof, based on skeleton
// comparison.
std::string GetMatchingSiteEngagementDomain(
    const std::vector<DomainInfo>& engaged_sites,
    const DomainInfo& navigated_domain) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  for (const DomainInfo& engaged_site : engaged_sites) {
    DCHECK(!engaged_site.domain_and_registry.empty());
    if (SkeletonsMatch(navigated_domain.skeletons, engaged_site.skeletons)) {
      return engaged_site.domain_and_registry;
    }
  }
  return std::string();
}

// Scans the top sites list and returns true if it finds a domain with an edit
// distance or character swap of one to |domain_and_registry|. This search is
// done in lexicographic order on the top 500 suitable domains, instead of in
// order by popularity. This means that the resulting "similar" domain may not
// be the most popular domain that matches.
bool GetSimilarDomainFromTopBucket(
    const DomainInfo& navigated_domain,
    const LookalikeTargetAllowlistChecker& target_allowlisted,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type) {
  TopBucketDomainsParams* top_bucket_domain_params = GetTopDomainParams();
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (size_t i = 0;
         i < top_bucket_domain_params->num_edit_distance_skeletons; i++) {
      const char* const top_domain_skeleton =
          top_bucket_domain_params->edit_distance_skeletons[i];
      DCHECK(strlen(top_domain_skeleton));
      // Check edit distance on skeletons.
      if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                  base::UTF8ToUTF16(top_domain_skeleton))) {
        const std::string top_domain =
            url_formatter::LookupSkeletonInTopDomains(
                top_domain_skeleton, url_formatter::SkeletonType::kFull)
                .domain;
        DCHECK(!top_domain.empty());

        if (!IsLikelyEditDistanceFalsePositive(navigated_domain,
                                               GetDomainInfo(top_domain)) &&
            !target_allowlisted.Run(top_domain)) {
          *matched_domain = top_domain;
          *match_type = LookalikeUrlMatchType::kEditDistance;
          return true;
        }
      }

      // Check character swap on skeletons.
      // TODO(crbug.com/40707797): Also check character swap on actual hostnames
      // with diacritics etc removed. This is because some characters have two
      // character skeletons such as m -> rn, and this prevents us from
      // detecting character swaps between example.com and exapmle.com.
      if (HasOneCharacterSwap(base::UTF8ToUTF16(navigated_skeleton),
                              base::UTF8ToUTF16(top_domain_skeleton))) {
        const std::string top_domain =
            url_formatter::LookupSkeletonInTopDomains(
                top_domain_skeleton, url_formatter::SkeletonType::kFull)
                .domain;
        DCHECK(!top_domain.empty());
        if (!IsLikelyCharacterSwapFalsePositive(navigated_domain,
                                                GetDomainInfo(top_domain)) &&
            !target_allowlisted.Run(top_domain)) {
          *matched_domain = top_domain;
          *match_type = LookalikeUrlMatchType::kCharacterSwapTop500;
          return true;
        }
      }
    }
  }
  return false;
}

// Scans the engaged site list for edit distance and character swap matches.
// Returns true if there is a match and fills |matched_domain| with the first
// matching engaged domain and |match_type| with the matching heuristic type.
bool GetSimilarDomainFromEngagedSites(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& target_allowlisted,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const DomainInfo& engaged_site : engaged_sites) {
      DCHECK_NE(navigated_domain.domain_and_registry,
                engaged_site.domain_and_registry);

      if (!url_formatter::top_domains::IsEditDistanceCandidate(
              engaged_site.domain_and_registry)) {
        continue;
      }
      // Skip past domains that are allowed to be spoofed.
      if (target_allowlisted.Run(engaged_site.domain_and_registry)) {
        continue;
      }
      for (const std::string& engaged_skeleton : engaged_site.skeletons) {
        // Check edit distance on skeletons.
        if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                    base::UTF8ToUTF16(engaged_skeleton)) &&
            !IsLikelyEditDistanceFalsePositive(navigated_domain,
                                               engaged_site)) {
          *matched_domain = engaged_site.domain_and_registry;
          *match_type = LookalikeUrlMatchType::kEditDistanceSiteEngagement;
          return true;
        }
        // Check character swap on skeletons.
        if (HasOneCharacterSwap(base::UTF8ToUTF16(navigated_skeleton),
                                base::UTF8ToUTF16(engaged_skeleton)) &&
            !IsLikelyCharacterSwapFalsePositive(navigated_domain,
                                                engaged_site)) {
          *matched_domain = engaged_site.domain_and_registry;
          *match_type = LookalikeUrlMatchType::kCharacterSwapSiteEngagement;
          return true;
        }
      }
    }
  }

  // Also check character swap on actual hostnames with diacritics etc removed.
  // This is because some characters have two character skeletons such as m ->
  // rn, and this prevents us from detecting character swaps between example.com
  // and exapmle.com.
  const std::u16string navigated_hostname_without_diacritics =
      url_formatter::MaybeRemoveDiacritics(navigated_domain.idn_result.result);
  if (navigated_hostname_without_diacritics !=
      navigated_domain.idn_result.result) {
    for (const DomainInfo& engaged_site : engaged_sites) {
      DCHECK_NE(navigated_domain.domain_and_registry,
                engaged_site.domain_and_registry);
      const std::u16string engaged_hostname_without_diacritics =
          url_formatter::MaybeRemoveDiacritics(engaged_site.idn_result.result);

      if (HasOneCharacterSwap(navigated_hostname_without_diacritics,
                              engaged_hostname_without_diacritics)) {
        *matched_domain = engaged_site.domain_and_registry;
        *match_type = LookalikeUrlMatchType::kCharacterSwapSiteEngagement;
        return true;
      }
    }
  }
  return false;
}

void RecordEvent(NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(lookalikes::kInterstitialHistogramName, event);
}

// Returns the parts of the domain that are separated by "." or "-", not
// including the eTLD.
//
// |hostname| must outlive the return value since the vector contains
// StringPieces.
std::vector<std::string_view> SplitDomainIntoTokens(
    const std::string& hostname) {
  return base::SplitStringPiece(hostname, kTargetEmbeddingSeparators,
                                base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

// Returns whether any subdomain ending in the last entry of |domain_labels| is
// allowlisted. e.g. if domain_labels = {foo,scholar,google,com}, checks the
// allowlist for google.com, scholar.google.com, and foo.scholar.google.com.
bool ASubdomainIsAllowlisted(
    const base::span<const std::string_view>& domain_labels,
    const LookalikeTargetAllowlistChecker& in_target_allowlist) {
  DCHECK(domain_labels.size() >= 2);
  std::string potential_hostname(domain_labels[domain_labels.size() - 1]);
  // Attach each token from the end to the embedded target to check if that
  // subdomain has been allowlisted.
  for (int i = domain_labels.size() - 2; i >= 0; i--) {
    potential_hostname =
        std::string(domain_labels[i]) + "." + potential_hostname;
    if (in_target_allowlist.Run(potential_hostname)) {
      return true;
    }
  }
  return false;
}

// Returns the top domain if the top domain without its separators matches the
// |potential_target| (e.g. googlecom). The matching is a skeleton matching.
std::string GetMatchingTopDomainWithoutSeparators(
    std::string_view potential_target) {
  const url_formatter::Skeletons skeletons =
      url_formatter::GetSkeletons(base::UTF8ToUTF16(potential_target));

  for (const auto& skeleton : skeletons) {
    url_formatter::TopDomainEntry matched_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kSeparatorsRemoved);
    if (!matched_domain.domain.empty() &&
        matched_domain.skeleton_type ==
            url_formatter::SkeletonType::kSeparatorsRemoved) {
      return matched_domain.domain;
    }
  }
  return std::string();
}

// Returns whether the visited domain is either for a bare eTLD+1 (e.g.
// 'google.com') or a trivial subdomain (e.g. 'www.google.com').
bool IsETLDPlusOneOrTrivialSubdomain(const DomainInfo& host) {
  return (host.domain_and_registry == host.hostname ||
          "www." + host.domain_and_registry == host.hostname);
}

// Returns if |etld_plus_one| shares the skeleton of an eTLD+1 with an engaged
// site or a top bucket domain. |embedded_target| is set to matching eTLD+1.
bool DoesETLDPlus1MatchTopDomainOrEngagedSite(
    const DomainInfo& domain,
    const std::vector<DomainInfo>& engaged_sites,
    std::string* embedded_target) {
  for (const auto& skeleton : domain.skeletons) {
    for (const auto& engaged_site : engaged_sites) {
      // Skeleton matching only calculates skeletons of the eTLD+1, so only
      // consider engaged sites that are bare eTLD+1s (or a trivial subdomain)
      // and are a skeleton match.
      if (IsETLDPlusOneOrTrivialSubdomain(engaged_site) &&
          base::Contains(engaged_site.skeletons, skeleton)) {
        *embedded_target = engaged_site.domain_and_registry;
        return true;
      }
    }
  }
  for (const auto& skeleton : domain.skeletons) {
    const url_formatter::TopDomainEntry top_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kFull);
    if (!top_domain.domain.empty() && top_domain.is_top_bucket) {
      *embedded_target = top_domain.domain;
      return true;
    }
  }
  return false;
}

// Returns whether the e2LD of the provided domain is a common word (e.g.
// weather.com, ask.com). Target embeddings of these domains are often false
// positives (e.g. "super-best-fancy-hotels.com" isn't spoofing "hotels.com").
bool UsesCommonWord(const reputation::SafetyTipsConfig* config_proto,
                    const DomainInfo& domain) {
  // kDomainsPermittedInEndEmbeddings are based on domains with common words,
  // but they should not be excluded here (and instead are checked later).
  for (auto* permitted_ending : kDomainsPermittedInEndEmbeddings) {
    if (domain.domain_and_registry == permitted_ending) {
      return false;
    }
  }

  // Search for words in the big common word list.
  if (url_formatter::common_words::IsCommonWord(
          domain.domain_without_registry)) {
    return true;
  }

  // Search for words in the component-provided word list.
  if (lookalikes::IsCommonWordInConfigProto(config_proto,
                                            domain.domain_without_registry)) {
    return true;
  }

  // Search for words in the local word lists.
  for (auto* common_word : kLocalAdditionalCommonWords) {
    if (domain.domain_without_registry == common_word) {
      return true;
    }
  }

  return false;
}

// Returns whether |domain_labels| is in the same domain as embedding_domain.
// e.g. IsEmbeddingItself(["foo", "example", "com"], "example.com") -> true
//  since foo.example.com is in the same domain as example.com.
bool IsEmbeddingItself(const base::span<const std::string_view>& domain_labels,
                       const std::string& embedding_domain) {
  DCHECK(domain_labels.size() >= 2);
  std::string potential_hostname(domain_labels[domain_labels.size() - 1]);
  // Attach each token from the end to the embedded target to check if that
  // subdomain is the embedding domain. (e.g. using the earlier example, check
  // each ["com", "example.com", "foo.example.com"] against "example.com".
  for (int i = domain_labels.size() - 2; i >= 0; i--) {
    potential_hostname =
        std::string(domain_labels[i]) + "." + potential_hostname;
    if (embedding_domain == potential_hostname) {
      return true;
    }
  }
  return false;
}

// Identical to url_formatter::top_domains::HostnameWithoutRegistry(), but
// respects de-facto public registries like .com.de using similar logic to
// GetETLDPlusOne. See kPrivateRegistriesTreatedAsPublic definition for more
// details. e.g. "google.com.de" returns "google". Call with an eTLD+1, not a
// full hostname.
std::string GetE2LDWithDeFactoPublicRegistries(
    const std::string& domain_and_registry) {
  if (domain_and_registry.empty()) {
    return std::string();
  }

  size_t registry_size =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          domain_and_registry.c_str(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  const size_t private_registry_size =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          domain_and_registry.c_str(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  // If the registry lengths are the same using public and private registries,
  // than this is just a public registry domain. Otherwise, we need to check if
  // the registry ends with one of our anointed registries.
  if (registry_size != private_registry_size) {
    for (const auto* private_registry : kPrivateRegistriesTreatedAsPublic) {
      if (base::EndsWith(domain_and_registry, private_registry)) {
        registry_size = private_registry_size;
      }
    }
  }

  std::string out =
      domain_and_registry.substr(0, domain_and_registry.size() - registry_size);
  base::TrimString(out, ".", &out);
  return out;
}

// Returns whether |embedded_target| and |embedding_domain| share the same e2LD,
// (as in, e.g., google.com and google.org, or airbnb.com.br and airbnb.com).
// Assumes |embedding_domain| is an eTLD+1. Respects de-facto public eTLDs.
bool IsCrossTLDMatch(const DomainInfo& embedded_target,
                     const std::string& embedding_domain) {
  return (
      GetE2LDWithDeFactoPublicRegistries(embedded_target.domain_and_registry) ==
      GetE2LDWithDeFactoPublicRegistries(embedding_domain));
}

// Returns whether |embedded_target| is one of kDomainsPermittedInEndEmbeddings
// and that |embedding_domain| ends with that domain, e.g. "evil-office.com" is
// permitted, as "office.com" is in kDomainsPermittedInEndEmbeddings.  Only
// impacts Target Embedding matches.
bool EndsWithPermittedDomains(const DomainInfo& embedded_target,
                              const std::string& embedding_domain) {
  for (auto* permitted_ending : kDomainsPermittedInEndEmbeddings) {
    if (embedded_target.domain_and_registry == permitted_ending &&
        base::EndsWith(embedding_domain,
                       base::StrCat({"-", permitted_ending}))) {
      return true;
    }
  }
  return false;
}

// A domain is allowed to be embedded if is embedding itself, if its e2LD is a
// common word, any valid partial subdomain is allowlisted, or if it's a
// cross-TLD match (e.g. google.com vs google.com.mx).
bool IsAllowedToBeEmbedded(
    const DomainInfo& embedded_target,
    const base::span<const std::string_view>& subdomain_span,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const std::string& embedding_domain,
    const reputation::SafetyTipsConfig* config_proto) {
  return UsesCommonWord(config_proto, embedded_target) ||
         ASubdomainIsAllowlisted(subdomain_span, in_target_allowlist) ||
         IsEmbeddingItself(subdomain_span, embedding_domain) ||
         IsCrossTLDMatch(embedded_target, embedding_domain) ||
         EndsWithPermittedDomains(embedded_target, embedding_domain);
}

// Returns the first character of the first string that is different from the
// second string. Strings should be at least 1 edit distance apart.
char GetFirstDifferentChar(const std::string& str1, const std::string& str2) {
  std::string::const_iterator i1 = str1.begin();
  std::string::const_iterator i2 = str2.begin();
  while (i1 != str1.end() && i2 != str2.end()) {
    if (*i1 != *i2) {
      return *i1;
    }
    i1++;
    i2++;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

// Brand names with length of 4 or less should not be checked in domains for
// Combo Squatting. Short brand names can cause false positives in results.
bool IsComboSquattingCandidate(const std::string& brand) {
  return brand.size() > kMinBrandNameLengthForComboSquatting;
}

// Extract brand names from engaged sites to be checked for Combo Squatting, if
// the brand is not one of the hard coded brand names.
std::vector<std::pair<std::string, std::string>> GetBrandNamesFromEngagedSites(
    const std::vector<DomainInfo>& engaged_sites) {
  std::vector<std::pair<std::string, std::string>> output;

  for (const DomainInfo& engaged_site : engaged_sites) {
    url_formatter::Skeletons domain_without_registry_skeletons =
        engaged_site.domain_without_registry_skeletons;
    for (const std::string& skeleton : domain_without_registry_skeletons)
      if (IsComboSquattingCandidate(engaged_site.domain_without_registry)) {
        std::pair<std::string, std::string> brand_name = {
            engaged_site.domain_without_registry, skeleton};
        output.emplace_back(brand_name);
      }
  }
  return output;
}

// Registry of the navigated domain is needed to find matched_domain
// in Combo Squatting domains. For example, registry of
// `google-login[.]co[.]br` is `co[.]br`.
std::string GetRegistry(const DomainInfo& navigated_domain) {
  size_t registry_size = navigated_domain.domain_and_registry.size() -
                         navigated_domain.domain_without_registry.size() - 1;

  std::string domain_and_registry = navigated_domain.domain_and_registry;
  std::string registry =
      domain_and_registry.substr(domain_and_registry.size() - registry_size,
                                 domain_and_registry.size() - 1);
  return registry;
}

// If a matched domain including the brand name and TLD of
// navigated domain is found in top domains, |matched_domain|
// is set to the found top domain. Otherwise, |matched_domain| will
// be set to brand_name[.]com. Hard coded brand names should be checked to have
// valid brand_name[.]com url.
std::string FindMatchedDomainForHardCodedComboSquatting(
    const std::string& brand_name,
    const DomainInfo& navigated_domain) {
  DomainInfo suggested_matched_domain =
      GetDomainInfo(brand_name + '.' + GetRegistry(navigated_domain));
  if (IsTopDomain(suggested_matched_domain)) {
    return suggested_matched_domain.hostname;
  } else {
    return brand_name + ".com";
  }
}

// Engaged sites are sorted based on engagement score, so |matched_domain|
// will be set to the first domain in the engaged sites lists that includes
// the brand name of the navigated domain.
std::string FindMatchedDomainForSiteEngagementComboSquatting(
    const std::string& brand_name,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites) {
  for (auto& engaged_site : engaged_sites) {
    if (brand_name == engaged_site.domain_without_registry) {
      return engaged_site.hostname;
    }
  }
  return std::string();
}

// Returns true if the navigated_domain is flagged as Combo Squatting.
// matched_domain is the suggested domain that will be shown to the user
// instead of the navigated_domain in the warning UI.
bool IsComboSquatting(
    const std::vector<std::pair<std::string, std::string>>& brand_names,
    const ComboSquattingParams& combo_squatting_params,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    std::string* matched_domain,
    bool is_hard_coded) {
  // Check if the domain has any brand name and any popular keyword.
  for (auto& brand : brand_names) {
    auto brand_name = brand.first;
    auto brand_skeleton = brand.second;
    DCHECK(IsComboSquattingCandidate(brand_name));
    for (auto& skeleton : navigated_domain.domain_without_registry_skeletons) {
      size_t brand_skeleton_pos = skeleton.find(brand_skeleton);
      if (skeleton.size() == brand_skeleton.size() ||
          brand_skeleton_pos == std::string::npos) {
        continue;
      }

      for (size_t j = 0; j < combo_squatting_params.num_popular_keywords; j++) {
        auto* const keyword = combo_squatting_params.popular_keywords[j];
        size_t keyword_pos = skeleton.find(keyword);
        if (keyword_pos == std::string::npos) {
          // Keyword not found, ignore.
          continue;
        }

        if (std::string(brand_skeleton).find(keyword) != std::string::npos ||
            std::string(keyword).find(brand_skeleton) != std::string::npos) {
          // Keyword is a substring of brand or vice versa, ignore.
          continue;
        }

        if ((keyword_pos > brand_skeleton_pos &&
             keyword_pos < brand_skeleton_pos + brand_skeleton.size()) ||
            (brand_skeleton_pos > keyword_pos &&
             brand_skeleton_pos < keyword_pos + strlen(keyword))) {
          // Keyword and brand overlap, ignore.
          continue;
        }

          if (is_hard_coded) {
            *matched_domain = FindMatchedDomainForHardCodedComboSquatting(
                brand_name, navigated_domain);
          } else {
            *matched_domain = FindMatchedDomainForSiteEngagementComboSquatting(
                brand_name, navigated_domain, engaged_sites);
          }
          return true;
      }
    }
  }
  return false;
}

}  // namespace

namespace lookalikes {

const char kInterstitialHistogramName[] = "NavigationSuggestion.Event2";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kLookalikeWarningAllowlistDomains);
}

std::string GetConsoleMessage(const GURL& lookalike_url,
                              bool is_new_heuristic) {
  const char* const kNewHeuristicMessage =
      "Future Chrome versions will show a warning on this domain name.\n";
  return base::StrCat({"Chrome has determined that ",
                       lookalike_url.host_piece(),
                       " could be fake or fraudulent.\n\n",
                       is_new_heuristic ? kNewHeuristicMessage : "",
                       "If you believe this is shown in error please visit "
                       "https://g.co/chrome/lookalike-warnings"});
}

DomainInfo::DomainInfo(
    const std::string& arg_hostname,
    const std::string& arg_domain_and_registry,
    const std::string& arg_domain_without_registry,
    const url_formatter::IDNConversionResult& arg_idn_result,
    const url_formatter::Skeletons& arg_skeletons,
    const url_formatter::Skeletons& arg_domain_without_registry_skeletons)
    : hostname(arg_hostname),
      domain_and_registry(arg_domain_and_registry),
      domain_without_registry(arg_domain_without_registry),
      idn_result(arg_idn_result),
      skeletons(arg_skeletons),
      domain_without_registry_skeletons(arg_domain_without_registry_skeletons) {
}

DomainInfo::~DomainInfo() = default;

DomainInfo::DomainInfo(const DomainInfo&) = default;

DomainInfo GetDomainInfo(const std::string& hostname) {
  TRACE_EVENT0("navigation", "GetDomainInfo");
  if (net::HostStringIsLocalhost(hostname) ||
      net::IsHostnameNonUnique(hostname)) {
    return DomainInfo(std::string(), std::string(), std::string(),
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons(), url_formatter::Skeletons());
  }
  const std::string domain_and_registry = GetETLDPlusOne(hostname);
  const std::string domain_without_registry =
      domain_and_registry.empty()
          ? std::string()
          : url_formatter::top_domains::HostnameWithoutRegistry(
                domain_and_registry);

  // eTLD+1 can be empty for private domains.
  if (domain_and_registry.empty()) {
    return DomainInfo(hostname, domain_and_registry, domain_without_registry,
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons(), url_formatter::Skeletons());
  }
  // Compute skeletons using eTLD+1, skipping all spoofing checks. Spoofing
  // checks in url_formatter can cause the converted result to be punycode.
  // We want to avoid this in order to get an accurate skeleton for the unicode
  // version of the domain.
  const url_formatter::IDNConversionResult idn_result =
      url_formatter::UnsafeIDNToUnicodeWithDetails(domain_and_registry);
  const url_formatter::Skeletons skeletons =
      url_formatter::GetSkeletons(idn_result.result);

  const url_formatter::IDNConversionResult domain_without_registry_idn_result =
      url_formatter::UnsafeIDNToUnicodeWithDetails(domain_without_registry);
  const url_formatter::Skeletons domain_without_registry_skeletons =
      url_formatter::GetSkeletons(domain_without_registry_idn_result.result);
  return DomainInfo(hostname, domain_and_registry, domain_without_registry,
                    idn_result, skeletons, domain_without_registry_skeletons);
}

DomainInfo GetDomainInfo(const GURL& url) {
  return GetDomainInfo(url.host());
}

std::string GetETLDPlusOne(const std::string& hostname) {
  auto pub = net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  auto priv = net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // If there is no difference in eTLD+1 with/without private registries, then
  // the domain uses a public registry and we can return the eTLD+1 safely.
  if (pub == priv) {
    return pub;
  }
  // Otherwise, the domain uses a private registry and |pub| is that private
  // registry. If it's a de-facto-public registry, return the private eTLD+1.
  for (auto* private_registry : kPrivateRegistriesTreatedAsPublic) {
    if (private_registry == pub) {
      return priv;
    }
  }
  // Otherwise, ignore the normal private registry and return the public eTLD+1.
  return pub;
}

bool IsEditDistanceAtMostOne(const std::u16string& str1,
                             const std::u16string& str2) {
  if (str1.size() > str2.size() + 1 || str2.size() > str1.size() + 1) {
    return false;
  }
  std::u16string::const_iterator i = str1.begin();
  std::u16string::const_iterator j = str2.begin();
  size_t edit_count = 0;
  while (i != str1.end() && j != str2.end()) {
    if (*i == *j) {
      i++;
      j++;
    } else {
      edit_count++;
      if (edit_count > 1) {
        return false;
      }

      if (str1.size() > str2.size()) {
        // First string is longer than the second. This can only happen if the
        // first string has an extra character.
        i++;
      } else if (str2.size() > str1.size()) {
        // Second string is longer than the first. This can only happen if the
        // second string has an extra character.
        j++;
      } else {
        // Both strings are the same length. This can only happen if the two
        // strings differ by a single character.
        i++;
        j++;
      }
    }
  }
  if (i != str1.end() || j != str2.end()) {
    // A character at the end did not match.
    edit_count++;
  }
  return edit_count <= 1;
}

bool IsLikelyEditDistanceFalsePositive(const DomainInfo& navigated_domain,
                                       const DomainInfo& matched_domain) {
  DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
      matched_domain.domain_and_registry));
  DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
      navigated_domain.domain_and_registry));
  // If the only difference between the domains is the registry part, this is
  // unlikely to be a spoofing attempt and we should ignore this match.  E.g.
  // exclude matches like google.com.tw and google.com.tr.
  if (navigated_domain.domain_without_registry ==
      matched_domain.domain_without_registry) {
    return true;
  }

  // If the domains only differ by a numeric suffix on their e2LD (e.g.
  // site45.tld and site35.tld), then ignore the match.
  auto nav_trimmed = base::TrimString(navigated_domain.domain_without_registry,
                                      kDigitChars, base::TRIM_TRAILING);
  auto matched_trimmed = base::TrimString(
      matched_domain.domain_without_registry, kDigitChars, base::TRIM_TRAILING);
  DCHECK_NE(navigated_domain.domain_without_registry,
            matched_domain.domain_without_registry);
  // We previously verified that the domains without registries weren't equal,
  // so if they're equal now, the match must have come from numeric suffixes.
  if (nav_trimmed == matched_trimmed) {
    return true;
  }

  // Ignore domains that only differ by an insertion/substitution at the
  // start, as these are usually different words, not lookalikes.
  const auto nav_dom_len = navigated_domain.domain_and_registry.length();
  const auto matched_dom_len = matched_domain.domain_and_registry.length();
  const auto& nav_dom = navigated_domain.domain_and_registry;
  const auto& matched_dom = matched_domain.domain_and_registry;
  if (nav_dom_len == matched_dom_len) {
    // e.g. hank vs tank
    if (nav_dom.substr(1) == matched_dom.substr(1)) {
      return true;
    }
  } else if (nav_dom_len < matched_dom_len) {
    // e.g. oodle vs poodle
    if (nav_dom == matched_dom.substr(1)) {
      return true;
    }
  } else {  // navigated_dom_len > matched_dom_len
    // e.g. poodle vs oodle
    if (nav_dom.substr(1) == matched_dom) {
      return true;
    }
  }

  // Ignore domains that only differ by an insertion of a "-".
  if (nav_dom_len != matched_dom_len) {
    if (nav_dom_len < matched_dom_len &&
        GetFirstDifferentChar(matched_dom, nav_dom) == '-') {
      return true;
    } else if (nav_dom_len > matched_dom_len &&
               GetFirstDifferentChar(nav_dom, matched_dom) == '-') {
      return true;
    }
  }

  return false;
}

bool IsLikelyCharacterSwapFalsePositive(const DomainInfo& navigated_domain,
                                        const DomainInfo& matched_domain) {
  DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
      matched_domain.domain_and_registry));
  DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
      navigated_domain.domain_and_registry));
  // If the only difference between the domains is the registry part, this is
  // unlikely to be a spoofing attempt and we should ignore this match.  E.g.
  // exclude matches like google.sr and google.rs.
  return navigated_domain.domain_without_registry ==
         matched_domain.domain_without_registry;
}

bool IsTopDomain(const DomainInfo& domain_info) {
  // Top domains are only accessible through their skeletons, so query the top
  // domains trie for each skeleton of this domain.
  for (const std::string& skeleton : domain_info.skeletons) {
    const url_formatter::TopDomainEntry top_domain =
        url_formatter::LookupSkeletonInTopDomains(
            skeleton, url_formatter::SkeletonType::kFull);
    if (domain_info.domain_and_registry == top_domain.domain) {
      return true;
    }
  }
  return false;
}

bool GetMatchingDomain(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    std::string* matched_domain,
    LookalikeUrlMatchType* match_type) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  DCHECK(matched_domain);
  DCHECK(match_type);

  if (navigated_domain.idn_result.has_idn_component) {
    // If the navigated domain is IDN, check its skeleton against engaged sites
    // and top domains.
    const std::string matched_engaged_domain =
        GetMatchingSiteEngagementDomain(engaged_sites, navigated_domain);
    DCHECK_NE(navigated_domain.domain_and_registry, matched_engaged_domain);
    if (!matched_engaged_domain.empty()) {
      *matched_domain = matched_engaged_domain;
      *match_type = LookalikeUrlMatchType::kSkeletonMatchSiteEngagement;
      return true;
    }

    if (!navigated_domain.idn_result.matching_top_domain.domain.empty()) {
      // In practice, this is not possible since the top domain list does not
      // contain IDNs, so domain_and_registry can't both have IDN and be a top
      // domain. Still, sanity check in case the top domain list changes in the
      // future.
      // At this point, navigated domain should not be a top domain.
      DCHECK_NE(navigated_domain.domain_and_registry,
                navigated_domain.idn_result.matching_top_domain.domain);
      *matched_domain = navigated_domain.idn_result.matching_top_domain.domain;
      *match_type =
          navigated_domain.idn_result.matching_top_domain.is_top_bucket
              ? LookalikeUrlMatchType::kSkeletonMatchTop500
              : LookalikeUrlMatchType::kSkeletonMatchTop5k;
      return true;
    }
  }

  if (url_formatter::top_domains::IsEditDistanceCandidate(
          navigated_domain.domain_and_registry)) {
    // If we can't find an exact top domain or an engaged site, try to find an
    // engaged domain within an edit distance of one or a single character swap.
    if (GetSimilarDomainFromEngagedSites(navigated_domain, engaged_sites,
                                         in_target_allowlist, matched_domain,
                                         match_type)) {
      DCHECK_NE(navigated_domain.domain_and_registry, *matched_domain);
      return true;
    }

    // Finally, try to find a top domain within an edit distance or character
    // swap of one.
    if (GetSimilarDomainFromTopBucket(navigated_domain, in_target_allowlist,
                                      matched_domain, match_type)) {
      DCHECK_NE(navigated_domain.domain_and_registry, *matched_domain);
      DCHECK(!matched_domain->empty());
      return true;
    }
  }

  TargetEmbeddingType embedding_type =
      GetTargetEmbeddingType(navigated_domain.hostname, engaged_sites,
                             in_target_allowlist, config_proto, matched_domain);
  if (embedding_type == TargetEmbeddingType::kSafetyTip) {
    *match_type = LookalikeUrlMatchType::kTargetEmbeddingForSafetyTips;
    return true;
  } else if (embedding_type == TargetEmbeddingType::kInterstitial) {
    *match_type = LookalikeUrlMatchType::kTargetEmbedding;
    return true;
  }

  // If none of the previous heuristics work, check it for Combo Squatting.
  ComboSquattingType combo_squatting_type =
      GetComboSquattingType(navigated_domain, engaged_sites, matched_domain);
  if (combo_squatting_type == ComboSquattingType::kHardCoded) {
    *match_type = LookalikeUrlMatchType::kComboSquatting;
    DCHECK(!matched_domain->empty());
    return true;
  } else if (combo_squatting_type == ComboSquattingType::kSiteEngagement) {
    *match_type = LookalikeUrlMatchType::kComboSquattingSiteEngagement;
    DCHECK(!matched_domain->empty());
    return true;
  }

  DCHECK(embedding_type == TargetEmbeddingType::kNone);
  DCHECK(combo_squatting_type == ComboSquattingType::kNone);
  return false;
}

void RecordUMAFromMatchType(LookalikeUrlMatchType match_type) {
  switch (match_type) {
    case LookalikeUrlMatchType::kSkeletonMatchSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchSiteEngagement);
      break;
    case LookalikeUrlMatchType::kEditDistance:
      RecordEvent(NavigationSuggestionEvent::kMatchEditDistance);
      break;
    case LookalikeUrlMatchType::kEditDistanceSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchEditDistanceSiteEngagement);
      break;
    case LookalikeUrlMatchType::kTargetEmbedding:
      RecordEvent(NavigationSuggestionEvent::kMatchTargetEmbedding);
      break;
    case LookalikeUrlMatchType::kSkeletonMatchTop500:
      RecordEvent(NavigationSuggestionEvent::kMatchSkeletonTop500);
      break;
    case LookalikeUrlMatchType::kSkeletonMatchTop5k:
      RecordEvent(NavigationSuggestionEvent::kMatchSkeletonTop5k);
      break;
    case LookalikeUrlMatchType::kTargetEmbeddingForSafetyTips:
      RecordEvent(
          NavigationSuggestionEvent::kMatchTargetEmbeddingForSafetyTips);
      break;
    case LookalikeUrlMatchType::kFailedSpoofChecks:
      RecordEvent(NavigationSuggestionEvent::kFailedSpoofChecks);
      break;
    case LookalikeUrlMatchType::kCharacterSwapSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchCharacterSwapSiteEngagement);
      break;
    case LookalikeUrlMatchType::kCharacterSwapTop500:
      RecordEvent(NavigationSuggestionEvent::kMatchCharacterSwapTop500);
      break;
    case LookalikeUrlMatchType::kComboSquatting:
      RecordEvent(NavigationSuggestionEvent::kComboSquatting);
      break;
    case LookalikeUrlMatchType::kComboSquattingSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kComboSquattingSiteEngagement);
      break;
    case LookalikeUrlMatchType::kNone:
      break;
  }
}

TargetEmbeddingType GetTargetEmbeddingType(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    std::string* safe_hostname) {
  // Because of how target embeddings are detected (i.e. by sweeping the URL
  // from back to front), we're guaranteed to find tail-embedding before other
  // target embedding. Tail embedding triggers a safety tip, but interstitials
  // are more important than safety tips, so if we find a safety tippable
  // embedding with SearchForEmbeddings, go search again not permitting safety
  // tips to see if we can also find an interstitiallable embedding.
  auto result = SearchForEmbeddings(
      hostname, engaged_sites, in_target_allowlist, config_proto,
      /*safety_tips_allowed=*/true, safe_hostname);
  if (result == TargetEmbeddingType::kSafetyTip) {
    std::string no_st_safe_hostname;
    auto no_st_result = SearchForEmbeddings(
        hostname, engaged_sites, in_target_allowlist, config_proto,
        /*safety_tips_allowed=*/false, &no_st_safe_hostname);
    if (no_st_result == TargetEmbeddingType::kNone) {
      return result;
    }
    *safe_hostname = no_st_safe_hostname;
    return no_st_result;
  }
  return result;
}

TargetEmbeddingType SearchForEmbeddings(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    const reputation::SafetyTipsConfig* config_proto,
    bool safety_tips_allowed,
    std::string* safe_hostname) {
  const std::string embedding_domain = GetETLDPlusOne(hostname);
  const std::vector<std::string_view> hostname_tokens =
      SplitDomainIntoTokens(hostname);

  // There are O(n^2) potential target embeddings in a domain name. We want to
  // be comprehensive, but optimize so that usually we needn't check all of
  // them. We do that by sweeping from the back of the embedding domain, towards
  // the front, checking for a valid eTLD. If we find one, then we consider the
  // possible embedded domains that end in that eTLD (i.e. all possible start
  // points from the beginning of the string onward).
  for (size_t end = hostname_tokens.size(); end > 0; --end) {
    base::span<const std::string_view> etld_check_span(hostname_tokens.data(),
                                                       end);
    std::string etld_check_host = base::JoinString(etld_check_span, ".");
    auto etld_check_dominfo = GetDomainInfo(etld_check_host);

    // Check if the final token is a no-separator target (e.g. "googlecom").
    // This check happens first so that we can exclude invalid eTLD+1s next.
    std::string embedded_target =
        GetMatchingTopDomainWithoutSeparators(hostname_tokens[end - 1]);
    if (!embedded_target.empty()) {
      // Extract the full possibly-spoofed domain. To get this, we take the
      // hostname up until this point, strip off the no-separator bit (e.g.
      // googlecom) and then re-add the the separated version (e.g. google.com).
      auto spoofed_domain =
          etld_check_host.substr(
              0, etld_check_host.length() - hostname_tokens[end - 1].length()) +
          embedded_target;
      const auto no_separator_tokens = base::SplitStringPiece(
          spoofed_domain, kTargetEmbeddingSeparators, base::TRIM_WHITESPACE,
          base::SPLIT_WANT_NONEMPTY);
      auto no_separator_dominfo = GetDomainInfo(embedded_target);

      // Only flag on domains that are long enough, don't use common words, and
      // aren't target-allowlisted.
      if (no_separator_dominfo.domain_without_registry.length() >
              kMinE2LDLengthForTargetEmbedding &&
          !IsAllowedToBeEmbedded(no_separator_dominfo, no_separator_tokens,
                                 in_target_allowlist, embedding_domain,
                                 config_proto)) {
        *safe_hostname = embedded_target;
        return TargetEmbeddingType::kInterstitial;
      }
    }

    // Exclude otherwise-invalid eTLDs.
    if (etld_check_dominfo.domain_without_registry.empty()) {
      continue;
    }

    // Exclude e2LDs that are too short. <= because domain_without_registry has
    // a trailing ".".
    if (etld_check_dominfo.domain_without_registry.length() <=
        kMinE2LDLengthForTargetEmbedding) {
      continue;
    }

    // Check for exact matches against engaged sites, among all possible
    // subdomains ending at |end|.
    for (size_t start = 0; start < end - 1; ++start) {
      const base::span<const std::string_view> span(
          hostname_tokens.data() + start, end - start);
      auto embedded_hostname = base::JoinString(span, ".");
      auto embedded_dominfo = GetDomainInfo(embedded_hostname);

      for (auto& engaged_site : engaged_sites) {
        if (engaged_site.hostname == embedded_dominfo.hostname &&
            !IsAllowedToBeEmbedded(embedded_dominfo, span, in_target_allowlist,
                                   embedding_domain, config_proto)) {
          *safe_hostname = engaged_site.hostname;
          // Tail-embedding (e.g. evil-google.com, where the embedding happens
          // at the very end of the hostname) is a safety tip, but only when
          // safety tips are allowed. If it's tail embedding but we can't create
          // a safety tip, keep looking.  Non-tail-embeddings are interstitials.
          if (end != hostname_tokens.size()) {
            return TargetEmbeddingType::kInterstitial;
          } else if (safety_tips_allowed) {
            return TargetEmbeddingType::kSafetyTip;
          }  // else keep searching.
        }
      }
    }

    // There were no exact engaged site matches, but there may yet still be a
    // match against the eTLD+1 of an engaged or top site.
    if (DoesETLDPlus1MatchTopDomainOrEngagedSite(
            etld_check_dominfo, engaged_sites, safe_hostname) &&
        !IsAllowedToBeEmbedded(etld_check_dominfo, etld_check_span,
                               in_target_allowlist, embedding_domain,
                               config_proto)) {
      // Tail-embedding (e.g. evil-google.com, where the embedding happens at
      // the very end of the hostname) is a safety tip, but only when safety
      // tips are allowed. If it's tail embedding but we can't create a safety
      // tip, keep looking.  Non-tail-embeddings are interstitials.
      if (end != hostname_tokens.size()) {
        return TargetEmbeddingType::kInterstitial;
      } else if (safety_tips_allowed) {
        return TargetEmbeddingType::kSafetyTip;
      }  // else keep searching.
    }
  }
  return TargetEmbeddingType::kNone;
}

bool IsASCII(UChar32 codepoint) {
  return !(codepoint & ~0x7F);
}

// Returns true if |codepoint| has emoji related properties.
bool IsEmojiRelatedCodepoint(UChar32 codepoint) {
  return u_hasBinaryProperty(codepoint, UCHAR_EMOJI) ||
         // Characters that have emoji presentation by default (e.g. hourglass)
         u_hasBinaryProperty(codepoint, UCHAR_EMOJI_PRESENTATION) ||
         // Characters displayed as country flags when used as a valid pair.
         // E.g. Regional Indicator Symbol Letter B used once in a string
         // is rendered as 🇧, used twice is rendered as the flag of Barbados
         // (with country code BB). It's therefore possible to come up with
         // a spoof using regional indicator characters as text, but these
         // domain names will be readily punycoded and detecting pairs isn't
         // easy so we keep the code simple here.
         u_hasBinaryProperty(codepoint, UCHAR_REGIONAL_INDICATOR) ||
         // Pictographs such as Black Cross On Shield (U+26E8).
         u_hasBinaryProperty(codepoint, UCHAR_EXTENDED_PICTOGRAPHIC);
}

// Returns true if |text| contains only ASCII characters, pictographs
// or emojis. This check is only used to determine if a domain that already
// failed spoof checks should be blocked by an interstitial. Ideally, we would
// check this for non-ASCII scripts as well (e.g. Cyrillic + emoji), but such
// usage isn't common.
bool IsASCIIAndEmojiOnly(std::u16string_view text) {
  for (base::i18n::UTF16CharIterator iter(text); !iter.end(); iter.Advance()) {
    const UChar32 codepoint = iter.get();
    if (!IsASCII(codepoint) && !IsEmojiRelatedCodepoint(codepoint)) {
      return false;
    }
  }
  return true;
}

// Returns true if the e2LD of domain is long enough to display a punycode
// interstitial.
bool IsPunycodeInterstitialCandidate(const DomainInfo& domain) {
  const url_formatter::IDNConversionResult idn_result =
      url_formatter::UnsafeIDNToUnicodeWithDetails(
          domain.domain_without_registry);
  return idn_result.result.size() >=
         kMinimumE2LDLengthToShowPunycodeInterstitial;
}

bool ShouldBlockBySpoofCheckResult(const DomainInfo& navigated_domain) {
  // Here, only a subset of spoof checks that cause an IDN to fallback to
  // punycode are configured to show an interstitial.
  switch (navigated_domain.idn_result.spoof_check_result) {
    case url_formatter::IDNSpoofChecker::Result::kNone:
    case url_formatter::IDNSpoofChecker::Result::kSafe:
      return false;

    case url_formatter::IDNSpoofChecker::Result::kICUSpoofChecks:
      // If the eTLD+1 contains only a mix of ASCII + Emoji, allow.
      return !IsASCIIAndEmojiOnly(navigated_domain.idn_result.result) &&
             IsPunycodeInterstitialCandidate(navigated_domain);

    case url_formatter::IDNSpoofChecker::Result::kDeviationCharacters:
      // Failures because of deviation characters, especially ß, is common.
      return false;

    case url_formatter::IDNSpoofChecker::Result::kTLDSpecificCharacters:
    case url_formatter::IDNSpoofChecker::Result::kUnsafeMiddleDot:
    case url_formatter::IDNSpoofChecker::Result::kWholeScriptConfusable:
    case url_formatter::IDNSpoofChecker::Result::kDigitLookalikes:
    case url_formatter::IDNSpoofChecker::Result::
        kNonAsciiLatinCharMixedWithNonLatin:
    case url_formatter::IDNSpoofChecker::Result::kDangerousPattern:
      return IsPunycodeInterstitialCandidate(navigated_domain);
  }
}

bool IsAllowedByEnterprisePolicy(const PrefService* pref_service,
                                 const GURL& url) {
  const base::Value::List& list =
      pref_service->GetList(prefs::kLookalikeWarningAllowlistDomains);

  for (const auto& domain_val : list) {
    const std::string& domain = domain_val.GetString();
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

void SetEnterpriseAllowlistForTesting(PrefService* pref_service,
                                      const std::vector<std::string>& hosts) {
  base::Value::List list;
  for (const auto& host : hosts) {
    list.Append(host);
  }
  pref_service->SetList(prefs::kLookalikeWarningAllowlistDomains,
                        std::move(list));
}

bool HasOneCharacterSwap(const std::u16string& str1,
                         const std::u16string& str2) {
  if (str1.size() != str2.size()) {
    return false;
  }
  if (str1 == str2) {
    return false;
  }
  bool has_swap = false;
  std::u16string::const_iterator i = str1.begin();
  std::u16string::const_iterator j = str2.begin();
  while (i != str1.end()) {
    DCHECK(j < str2.end());
    wchar_t left1 = *i;
    wchar_t right1 = *j;
    i++;
    j++;
    if (left1 == right1) {
      continue;
    }
    wchar_t left2 = *i;
    wchar_t right2 = *j;
    if (!has_swap && (left1 == right2 && right1 == left2)) {
      has_swap = true;
      i++;
      j++;
      continue;
    }
    // Either there are multiple swaps, or strings have completely different
    // characters.
    return false;
  }
  return has_swap;
}

void SetTopBucketDomainsParamsForTesting(const TopBucketDomainsParams& params) {
  *GetTopDomainParams() = params;
}

void ResetTopBucketDomainsParamsForTesting() {
  TopBucketDomainsParams* params = GetTopDomainParams();
  *params = {top_bucket_domains::kTopBucketEditDistanceSkeletons,
             top_bucket_domains::kNumTopBucketEditDistanceSkeletons};
}

bool IsHeuristicEnabledForHostname(
    const reputation::SafetyTipsConfig* config_proto,
    const reputation::HeuristicLaunchConfig::Heuristic heuristic,
    const std::string& lookalike_etld_plus_one,
    version_info::Channel channel) {
  DCHECK(!lookalike_etld_plus_one.empty());
  if (!config_proto) {
    return false;
  }
  base::SHA1Digest hash =
      base::SHA1Hash(base::as_byte_span(lookalike_etld_plus_one));
  float cohort = hash[0u] / 2.56;
  for (const reputation::HeuristicLaunchConfig& config :
       config_proto->launch_config()) {
    if (heuristic == config.heuristic()) {
      switch (channel) {
        // Enable by default on local builds.
        case version_info::Channel::UNKNOWN:
          return true;

        // Use pre-defined launch percentages for Canary/Dev and Beta. Use the
        // launch percentage from config for Stable.
        case version_info::Channel::CANARY:
        case version_info::Channel::DEV:
          return kDefaultLaunchPercentageOnCanaryDev > cohort;

        case version_info::Channel::BETA:
          return kDefaultLaunchPercentageOnBeta > cohort;

        case version_info::Channel::STABLE:
          return config.launch_percentage() > cohort;
      }
    }
  }
  return false;
}

void SetComboSquattingParamsForTesting(const ComboSquattingParams& params) {
  *GetComboSquattingParams() = params;
}

void ResetComboSquattingParamsForTesting() {
  ComboSquattingParams* params = GetComboSquattingParams();
  *params = {kBrandNamesForCSQ, std::size(kBrandNamesForCSQ),
             kSkeletonsOfPopularKeywordsForCSQ,
             std::size(kSkeletonsOfPopularKeywordsForCSQ)};
}

ComboSquattingType GetComboSquattingType(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    std::string* matched_domain) {
  const ComboSquattingParams* combo_squatting_params =
      GetComboSquattingParams();

  // First check Combo Squatting with hard coded brand names.
  std::vector<std::pair<std::string, std::string>> brand_names;
  for (size_t i = 0; i < combo_squatting_params->num_brand_names; i++) {
    brand_names.emplace_back(combo_squatting_params->brand_names[i]);
  }
  if (IsComboSquatting(brand_names, *combo_squatting_params, navigated_domain,
                       engaged_sites, matched_domain,
                       /*is_hard_coded=*/true)) {
    return ComboSquattingType::kHardCoded;
  }

  // Then check Combo Squatting with brand names in engaged sites.
  brand_names = GetBrandNamesFromEngagedSites(engaged_sites);
  if (IsComboSquatting(brand_names, *combo_squatting_params, navigated_domain,
                       engaged_sites, matched_domain,
                       /*is_hard_coded=*/false)) {
    return ComboSquattingType::kSiteEngagement;
  }

  return ComboSquattingType::kNone;
}

bool IsSafeTLD(const std::string& hostname) {
  // This is intentionally kept simple and currently ignores hostnames with
  // ccTLDs (e.g. gov.in).
  return base::EndsWith(hostname, ".gov") || base::EndsWith(hostname, ".mil");
}

LookalikeActionType GetActionForMatchType(
    const reputation::SafetyTipsConfig* config,
    version_info::Channel channel,
    const std::string& etld_plus_one,
    LookalikeUrlMatchType match_type) {
  switch (match_type) {
    case LookalikeUrlMatchType::kEditDistance:
      // Edit distance is too noisy, just record metrics.
      return LookalikeActionType::kRecordMetrics;

    case LookalikeUrlMatchType::kEditDistanceSiteEngagement:
      return LookalikeActionType::kShowSafetyTip;

    case LookalikeUrlMatchType::kTargetEmbedding:
#if BUILDFLAG(IS_IOS)
      // TODO(crbug.com/40705070): Only enable target embedding on iOS once we
      // can
      //    check engaged sites. Otherwise, false positives are too high.
      return LookalikeActionType::kRecordMetrics;
#else
      return LookalikeActionType::kShowInterstitial;
#endif

    case LookalikeUrlMatchType::kTargetEmbeddingForSafetyTips:
      return LookalikeActionType::kShowSafetyTip;

    case LookalikeUrlMatchType::kSkeletonMatchTop5k:
      return LookalikeActionType::kShowSafetyTip;

    case LookalikeUrlMatchType::kFailedSpoofChecks:
      return LookalikeActionType::kShowInterstitial;

    case LookalikeUrlMatchType::kSkeletonMatchSiteEngagement:
    case LookalikeUrlMatchType::kSkeletonMatchTop500:
      return LookalikeActionType::kShowInterstitial;

    case LookalikeUrlMatchType::kCharacterSwapSiteEngagement:
    case LookalikeUrlMatchType::kCharacterSwapTop500:
      return LookalikeActionType::kShowSafetyTip;

    case LookalikeUrlMatchType::kComboSquatting:
      return IsHeuristicEnabledForHostname(
                 config,
                 reputation::HeuristicLaunchConfig::
                     HEURISTIC_COMBO_SQUATTING_TOP_DOMAINS,
                 etld_plus_one, channel)
                 ? LookalikeActionType::kShowSafetyTip
                 : LookalikeActionType::kRecordMetrics;

    case LookalikeUrlMatchType::kComboSquattingSiteEngagement:
      return IsHeuristicEnabledForHostname(
                 config,
                 reputation::HeuristicLaunchConfig::
                     HEURISTIC_COMBO_SQUATTING_ENGAGED_SITES,
                 etld_plus_one, channel)
                 ? LookalikeActionType::kShowSafetyTip
                 : LookalikeActionType::kRecordMetrics;

    case LookalikeUrlMatchType::kNone:
      NOTREACHED_IN_MIGRATION();
  }

  NOTREACHED_IN_MIGRATION();
  return LookalikeActionType::kNone;
}

GURL GetSuggestedURL(LookalikeUrlMatchType match_type,
                     const GURL& navigated_url,
                     const std::string& matched_hostname) {
  // matched_hostname can be a top domain or an engaged domain. Simply use its
  // eTLD+1 as the suggested domain.
  // 1. If matched_hostname is a top domain: Top domain list already contains
  // eTLD+1s only so this works well.
  // 2. If matched_hostname is an engaged domain and is not an eTLD+1, don't
  // suggest it. Otherwise, navigating to googlé.com and having engaged with
  // docs.google.com would suggest docs.google.com.
  //
  // When the navigated and matched domains are not eTLD+1s (e.g.
  // docs.googlé.com and docs.google.com), this will suggest google.com
  // instead of docs.google.com. This is less than ideal, but has two
  // benefits:
  // - Simpler code
  // - Fewer suggestions to non-existent domains. E.g. When the navigated
  // domain is nonexistent.googlé.com and the matched domain is
  // docs.google.com, we will suggest google.com instead of
  // nonexistent.google.com.
  std::string suggested_domain = GetETLDPlusOne(matched_hostname);
  DCHECK(!suggested_domain.empty());
  // Drop everything but the parts of the origin.
  GURL::Replacements replace_host;
  replace_host.SetHostStr(suggested_domain);
  GURL suggested_url =
      navigated_url.ReplaceComponents(replace_host).GetWithEmptyPath();

  // Use https for top domain matches.
  // TODO(crbug.com/40755923): If the match is against an engaged site, use the
  // scheme of the engaged site instead.
  if (suggested_url.SchemeIs(url::kHttpScheme) &&
      suggested_url.IntPort() == url::PORT_UNSPECIFIED &&
      (match_type == LookalikeUrlMatchType::kEditDistance ||
       match_type == LookalikeUrlMatchType::kSkeletonMatchTop500 ||
       match_type == LookalikeUrlMatchType::kSkeletonMatchTop5k)) {
    GURL::Replacements replace_scheme;
    replace_scheme.SetSchemeStr(url::kHttpsScheme);
    suggested_url = suggested_url.ReplaceComponents(replace_scheme);
  }
  return suggested_url;
}

}  // namespace lookalikes
