// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filter.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/safe_search_api/url_checker.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_capabilities.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/url_matcher/url_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
using net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using net::registry_controlled_domains::GetCanonicalHostRegistryLength;

namespace supervised_user {

supervised_user::FilteringBehavior GetBehaviorFromSafeSearchClassification(
    safe_search_api::Classification classification) {
  switch (classification) {
    case safe_search_api::Classification::SAFE:
      return FilteringBehavior::kAllow;
    case safe_search_api::Classification::UNSAFE:
      return FilteringBehavior::kBlock;
  }
  NOTREACHED_IN_MIGRATION();
  return FilteringBehavior::kBlock;
}

bool IsSameDomain(const GURL& url1, const GURL& url2) {
  return net::registry_controlled_domains::SameDomainOrHost(
      url1, url2, EXCLUDE_PRIVATE_REGISTRIES);
}

bool IsNonStandardUrlScheme(const GURL& effective_url) {
  // URLs with a non-standard scheme (e.g. chrome://) are always allowed.
  return !effective_url.SchemeIsHTTPOrHTTPS() &&
         !effective_url.SchemeIsWSOrWSS() &&
         !effective_url.SchemeIs(url::kFtpScheme);
}

bool IsAlwaysAllowedHost(const GURL& effective_url) {
  // Allow navigations to allowed origins.
  constexpr auto kAllowedHosts = base::MakeFixedFlatSet<std::string_view>(
      {"accounts.google.com", "families.google.com", "familylink.google.com",
       "myaccount.google.com", "ogs.google.com", "policies.google.com",
       "support.google.com"});

  return base::Contains(kAllowedHosts, effective_url.host_piece());
}

bool IsAlwaysAllowedUrlPrefix(const GURL& effective_url) {
  // A list of allowed URL prefixes.
  //
  // Consider using url_matcher::CreateURLPrefixCondition (initialized once at
  // startup) for performance if the set of allowed URL prefixes grows large.
  static const char* const kAllowedUrlPrefixes[] = {
      // The Chrome sync dashboard is linked to from within Chrome settings.
      // Allow both the initial URL that is loaded, and the URL to which it
      // redirects.
      kSyncGoogleDashboardURL, "https://chrome.google.com/sync"};

  for (const char* allowedUrlPrefix : kAllowedUrlPrefixes) {
    if (base::StartsWith(effective_url.spec(), allowedUrlPrefix)) {
      return true;
    }
  }
  return false;
}

bool IsPlayStoreTermsOfServiceUrl(const GURL& effective_url) {
  // Play Store terms of service path:
  // TODO(b/322186372): Remove old host and path after some time.
  static const char* kPlayStoreHostOld = "play.google.com";
  static const char* kPlayStoreHostNew = "play.google";
  static const char* kPlayTermsPathOld = "/about/play-terms";
  static const char* kPlayTermsPathNew = "/play-terms";
  // Check Play Store terms of service.
  // path_piece is checked separately from the host to match international pages
  // like https://play.google.com/intl/pt-BR_pt/about/play-terms/ or
  // https://play.google/intl/pt-BR_pt/play-terms/.
  return effective_url.SchemeIs(url::kHttpsScheme) &&
         ((effective_url.host_piece() == kPlayStoreHostOld &&
           (effective_url.path_piece().find(kPlayTermsPathOld) !=
            std::string_view::npos)) ||
          (effective_url.host_piece() == kPlayStoreHostNew &&
           (effective_url.path_piece().find(kPlayTermsPathNew) !=
            std::string_view::npos)));
}

namespace {

// UMA histogram FamilyUser.ManagedSiteList.Conflict
// Reports conflict when the user tries to access a url that has a match in
// both of the allow list and the block list.
const char kManagedSiteListConflictHistogramName[] =
    "FamilyUser.ManagedSiteList.Conflict";

// UMA histogram FamilyUser.ManagedSiteList.SubdomainConflictType
// Reports a conflict when the user tries to access a url that has a match in
// both of the allow list and the block list and the two conflicting entries
// differs only in the "www" subdomain.
const char kManagedSiteListSubdomainConflictTypeHistogramName[] =
    "FamilyUser.ManagedSiteList.SubdomainConflictType";

// UMA histogram FamilyUser.WebFilterType
// Reports WebFilterType which indicates web filter behaviour are used for
// current Family Link user.
constexpr char kWebFilterTypeHistogramName[] = "FamilyUser.WebFilterType";

// UMA histogram FamilyUser.ManualSiteListType
// Reports ManualSiteListType which indicates approved list and blocked list
// usage for current Family Link user.
constexpr char kManagedSiteListHistogramName[] = "FamilyUser.ManagedSiteList";

// UMA histogram FamilyUser.ManagedSiteListCount.Approved
// Reports the number of approved urls and domains for current Family Link user.
constexpr char kApprovedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Approved";

// UMA histogram FamilyUser.ManagedSiteListCount.Blocked
// Reports the number of blocked urls and domains for current Family Link user.
constexpr char kBlockedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Blocked";

constexpr std::string_view kHttpProtocol = "http://";
constexpr std::string_view kHttpsProtocol = "https://";
constexpr std::string_view kWwwSubdomain = "www.";

// Trims the given `pattern` if it starts with 'https://' or 'http://'.
std::string TrimHttpOrHttpsProtocol(const std::string& pattern) {
  std::string trimmed_pattern = pattern;
  if (base::StartsWith(pattern, kHttpsProtocol, base::CompareCase::SENSITIVE)) {
    trimmed_pattern = pattern.substr(kHttpsProtocol.size());
  } else if (base::StartsWith(pattern, kHttpProtocol,
                              base::CompareCase::SENSITIVE)) {
    trimmed_pattern = pattern.substr(kHttpProtocol.size());
  }
  return trimmed_pattern;
}

// Trims 'www' subdomain if it is present on the given pattern.
std::string TrimWwwSubdomain(const std::string& pattern) {
  std::string trimmed_pattern = pattern;
  if (base::StartsWith(pattern, kWwwSubdomain)) {
    trimmed_pattern = pattern.substr(kWwwSubdomain.size());
  }
  return trimmed_pattern;
}

// For a given host `pattern` and, checks if there is another pattern in
// the given `host_list` which differs only in the trivial
// "www"-subdomain.
// Note: This method applies transformations to the given pattern
// opposite to those from `SupervisedUserURLFilter::HostMatchesPattern`
// (e.g. protocol/subdomain stripping). The pattern manipulations should
// be kept in sync between the two methods.
bool HostHasTrivialSubdomainConflict(const std::string& pattern,
                                     const std::set<std::string> host_list) {
  if (base::StartsWith(pattern, "*.")) {
    return false;
  }

  std::string removed_protocol_pattern = TrimHttpOrHttpsProtocol(pattern);
  std::string removed_subdomain_pattern =
      TrimWwwSubdomain(removed_protocol_pattern);

  bool has_www_subdomain =
      removed_protocol_pattern != removed_subdomain_pattern;
  std::string subdomain_replacement =
      has_www_subdomain ? std::string() : kWwwSubdomain.data();

  return base::Contains(host_list,
                        subdomain_replacement + removed_subdomain_pattern) ||
         base::Contains(host_list, kHttpProtocol.data() +
                                       subdomain_replacement +
                                       removed_subdomain_pattern) ||
         base::Contains(host_list, kHttpsProtocol.data() +
                                       subdomain_replacement +
                                       removed_subdomain_pattern);
}

using FilteringSubdomainConflictType =
    SupervisedUserURLFilter::FilteringSubdomainConflictType;

std::optional<FilteringSubdomainConflictType> AddConflict(
    std::optional<FilteringSubdomainConflictType> current_conflict,
    bool is_trivial_subdomain_conflict) {
  if (!current_conflict.has_value()) {
    return is_trivial_subdomain_conflict
               ? FilteringSubdomainConflictType::kTrivialSubdomainConflictOnly
               : FilteringSubdomainConflictType::kOtherConflictOnly;
  }
  switch (current_conflict.value()) {
    case FilteringSubdomainConflictType::kTrivialSubdomainConflictOnly:
      return is_trivial_subdomain_conflict
                 ? FilteringSubdomainConflictType::kTrivialSubdomainConflictOnly
                 : FilteringSubdomainConflictType::
                       kTrivialSubdomainConflictAndOtherConflict;
    case FilteringSubdomainConflictType::kOtherConflictOnly:
      return is_trivial_subdomain_conflict
                 ? FilteringSubdomainConflictType::
                       kTrivialSubdomainConflictAndOtherConflict
                 : FilteringSubdomainConflictType::kOtherConflictOnly;
    case FilteringSubdomainConflictType::
        kTrivialSubdomainConflictAndOtherConflict:
      return FilteringSubdomainConflictType::
          kTrivialSubdomainConflictAndOtherConflict;
  }
}

}  // namespace

SupervisedUserURLFilter::SupervisedUserURLFilter(
    PrefService& user_prefs,
    std::unique_ptr<Delegate> delegate)
    : default_behavior_(FilteringBehavior::kAllow),
      user_prefs_(user_prefs),
      delegate_(std::move(delegate)) {}

SupervisedUserURLFilter::~SupervisedUserURLFilter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
const char* SupervisedUserURLFilter::GetWebFilterTypeHistogramNameForTest() {
  return kWebFilterTypeHistogramName;
}

// static
const char* SupervisedUserURLFilter::GetManagedSiteListHistogramNameForTest() {
  return kManagedSiteListHistogramName;
}

// static
const char*
SupervisedUserURLFilter::GetApprovedSitesCountHistogramNameForTest() {
  return kApprovedSitesCountHistogramName;
}

// static
const char*
SupervisedUserURLFilter::GetBlockedSitesCountHistogramNameForTest() {
  return kBlockedSitesCountHistogramName;
}

// static
const char*
SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest() {
  return kManagedSiteListConflictHistogramName;
}

// static
const char*
SupervisedUserURLFilter::GetManagedSiteListConflictTypeHistogramNameForTest() {
  return kManagedSiteListSubdomainConflictTypeHistogramName;
}

// static
supervised_user::FilteringBehavior SupervisedUserURLFilter::BehaviorFromInt(
    int behavior_value) {
  // `behavior_value` is external input (from the server) - do not turn
  // DCHECK into CHECK as it might lead to real crashes if the server ever
  // supplies an unsupported value.
  DCHECK(behavior_value == static_cast<int>(FilteringBehavior::kAllow) ||
         behavior_value == static_cast<int>(FilteringBehavior::kBlock))
      << "SupervisedUserURLFilter value not supported: " << behavior_value;
  return static_cast<FilteringBehavior>(behavior_value);
}

// static
// Note: The transformations applied to pattern (e.g. protocol/subdomain
// stripping) should be kept in sync with those in the method
// `HostHasSubdomainConflict`.
bool SupervisedUserURLFilter::HostMatchesPattern(
    const std::string& canonical_host,
    const std::string& pattern) {
  std::string trimmed_host = canonical_host;
  std::string trimmed_pattern = TrimHttpOrHttpsProtocol(pattern);

  bool host_starts_with_www = base::StartsWith(canonical_host, kWwwSubdomain,
                                               base::CompareCase::SENSITIVE);
  bool pattern_starts_with_www = base::StartsWith(
      trimmed_pattern, kWwwSubdomain, base::CompareCase::SENSITIVE);

  // Trim the initial "www." if it appears on either the host or the pattern,
  // but not if it appears on both.
  if (host_starts_with_www != pattern_starts_with_www) {
    if (host_starts_with_www) {
      trimmed_host = TrimWwwSubdomain(trimmed_host);
    } else if (pattern_starts_with_www) {
      trimmed_pattern = TrimWwwSubdomain(trimmed_pattern);
    }
  }

  if (base::EndsWith(pattern, ".*", base::CompareCase::SENSITIVE)) {
    size_t registry_length = GetCanonicalHostRegistryLength(
        trimmed_host, EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES);
    // A host without a known registry part does not match.
    if (registry_length == 0) {
      return false;
    }

    trimmed_pattern.erase(trimmed_pattern.length() - 2);
    trimmed_host.erase(trimmed_host.length() - (registry_length + 1));
  }

  if (base::StartsWith(trimmed_pattern, "*.", base::CompareCase::SENSITIVE)) {
    trimmed_pattern.erase(0, 2);

    // The remaining pattern should be non-empty, and it should not contain
    // further stars. Also the trimmed host needs to end with the trimmed
    // pattern.
    if (trimmed_pattern.empty() ||
        trimmed_pattern.find('*') != std::string::npos ||
        !base::EndsWith(trimmed_host, trimmed_pattern,
                        base::CompareCase::SENSITIVE)) {
      return false;
    }

    // The trimmed host needs to have a dot separating the subdomain from the
    // matched pattern piece, unless there is no subdomain.
    int pos = trimmed_host.length() - trimmed_pattern.length();
    DCHECK_GE(pos, 0);
    return (pos == 0) || (trimmed_host[pos - 1] == '.');
  }

  return trimmed_host == trimmed_pattern;
}

SupervisedUserFilterTopLevelResult
SupervisedUserURLFilter::GetHistogramValueForTopLevelFilteringBehavior(
    FilteringBehavior behavior,
    FilteringBehaviorReason reason,
    bool is_filtering_behavior_known) {
  switch (behavior) {
    case FilteringBehavior::kAllow:
      return SupervisedUserFilterTopLevelResult::kAllow;
    case FilteringBehavior::kBlock:
      switch (reason) {
        case FilteringBehaviorReason::ASYNC_CHECKER:
          return SupervisedUserFilterTopLevelResult::kBlockSafeSites;
        case FilteringBehaviorReason::MANUAL:
          return SupervisedUserFilterTopLevelResult::kBlockManual;
        case FilteringBehaviorReason::DEFAULT:
          return SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist;
      }
    case FilteringBehavior::kInvalid:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return SupervisedUserFilterTopLevelResult::kAllow;
}

// static
int SupervisedUserURLFilter::GetHistogramValueForFilteringBehavior(
    FilteringBehavior behavior,
    FilteringBehaviorReason reason,
    bool is_filtering_behavior_known) {
  switch (behavior) {
    case FilteringBehavior::kAllow:
      return is_filtering_behavior_known
                 ? SupervisedUserSafetyFilterResult::FILTERING_BEHAVIOR_ALLOW
                 : SupervisedUserSafetyFilterResult::
                       FILTERING_BEHAVIOR_ALLOW_UNCERTAIN;
    case FilteringBehavior::kBlock:
      switch (reason) {
        case FilteringBehaviorReason::ASYNC_CHECKER:
          return SupervisedUserSafetyFilterResult::
              FILTERING_BEHAVIOR_BLOCK_SAFESITES;
        case FilteringBehaviorReason::MANUAL:
          return SupervisedUserSafetyFilterResult::
              FILTERING_BEHAVIOR_BLOCK_MANUAL;
        case FilteringBehaviorReason::DEFAULT:
          return SupervisedUserSafetyFilterResult::
              FILTERING_BEHAVIOR_BLOCK_DEFAULT;
      }
    case FilteringBehavior::kInvalid:
      NOTREACHED_IN_MIGRATION();
  }
  return 0;
}

// static
void SupervisedUserURLFilter::RecordFilterResultEvent(
    FilteringBehavior behavior,
    FilteringBehaviorReason reason,
    bool is_filtering_behavior_known,
    ui::PageTransition transition_type) {
  SupervisedUserFilterTopLevelResult top_level_filter_behaviour =
      GetHistogramValueForTopLevelFilteringBehavior(
          behavior, reason, is_filtering_behavior_known);
  base::UmaHistogramSparse(
      kSupervisedUserTopLevelURLFilteringResultHistogramName,
      static_cast<int>(top_level_filter_behaviour));

  int value = GetHistogramValueForFilteringBehavior(
                  behavior, reason, is_filtering_behavior_known) *
                  kHistogramFilteringBehaviorSpacing +
              GetHistogramValueForTransitionType(transition_type);
  DCHECK_LT(value, kSupervisedUserURLFilteringResultHistogramMax);
  base::UmaHistogramSparse(kSupervisedUserURLFilteringResultHistogramName,
                           value);
}

supervised_user::FilteringBehavior
SupervisedUserURLFilter::GetFilteringBehaviorForURL(const GURL& url) {
  supervised_user::FilteringBehaviorReason reason;
  return GetFilteringBehaviorForURL(url, &reason);
}

bool SupervisedUserURLFilter::IsExemptedFromGuardianApproval(
    const GURL& effective_url) {
  DCHECK(delegate_);
  return IsNonStandardUrlScheme(effective_url) ||
         IsAlwaysAllowedHost(effective_url) ||
         IsAlwaysAllowedUrlPrefix(effective_url) ||
         IsPlayStoreTermsOfServiceUrl(effective_url) ||
         delegate_->SupportsWebstoreURL(effective_url);
}

bool SupervisedUserURLFilter::GetManualFilteringBehaviorForURL(
    const GURL& url,
    FilteringBehavior* behavior) {
  supervised_user::FilteringBehaviorReason reason;
  *behavior = GetFilteringBehaviorForURL(url, &reason);
  return reason == supervised_user::FilteringBehaviorReason::MANUAL;
}

supervised_user::FilteringBehavior
SupervisedUserURLFilter::GetFilteringBehaviorForURL(
    const GURL& url,
    supervised_user::FilteringBehaviorReason* reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GURL effective_url = url_matcher::util::GetEmbeddedURL(url);
  if (!effective_url.is_valid()) {
    effective_url = url;
  }

  *reason = supervised_user::FilteringBehaviorReason::MANUAL;

  if (IsExemptedFromGuardianApproval(effective_url)) {
    return FilteringBehavior::kAllow;
  }

  // Check manual denylists and allowlists.
  FilteringBehavior manual_result =
      GetManualFilteringBehaviorForURL(effective_url);
  if (manual_result != FilteringBehavior::kInvalid) {
    return manual_result;
  }

  // Fall back to the default behavior.
  *reason = supervised_user::FilteringBehaviorReason::DEFAULT;
  return default_behavior_;
}

// There may be conflicting patterns, say, "allow *.google.com" and "block
// www.google.*". To break the tie, we prefer blocklists over allowlists.
// If there are no applicable manual overrides, we return INVALID.
supervised_user::FilteringBehavior
SupervisedUserURLFilter::GetManualFilteringBehaviorForURL(const GURL& url) {
  FilteringBehavior result = FilteringBehavior::kInvalid;
  std::optional<FilteringSubdomainConflictType> conflict_type = std::nullopt;

  // Records the conflict metrics when the current scope exits.
  absl::Cleanup histogram_recorder = [&result, &conflict_type] {
    if (result != FilteringBehavior::kInvalid) {
      // Record the potential conflict and its type.
      bool conflict = conflict_type.has_value();
      UMA_HISTOGRAM_BOOLEAN(kManagedSiteListConflictHistogramName, conflict);
      if (conflict) {
        base::UmaHistogramEnumeration(
            kManagedSiteListSubdomainConflictTypeHistogramName,
            conflict_type.value());
      }
    }
  };

  // Check manual overrides for the exact URL.
  auto url_it = url_map_.find(url_matcher::util::Normalize(url));
  if (url_it != url_map_.end()) {
    result =
        url_it->second ? FilteringBehavior::kAllow : FilteringBehavior::kBlock;
  }

  const std::string host = url.host();
  if (result != FilteringBehavior::kBlock) {
    // If there is a match with Block behaviour, set the result to Block.
    auto it = base::ranges::find_if(
        blocked_host_list_, [&host](const std::string& host_entry) {
          return HostMatchesPattern(host, host_entry);
        });
    if (it != blocked_host_list_.end()) {
      result = FilteringBehavior::kBlock;
    }
  }

  if (result == FilteringBehavior::kAllow) {
    // Return if there are no BLOCK matches and the resut is ALLOW from the
    // exact url search.
    return result;
  }

  // If there the result is not set to Block, look for matching allowed entries.
  // If the result is Block, detect potential conflicts.
  for (const auto& host_entry : allowed_host_list_) {
    if (!HostMatchesPattern(host, host_entry)) {
      continue;
    }
    if (result == FilteringBehavior::kInvalid) {
      // If the result is still unset, there are no conflicts from the
      // blocklist. Set the result and exit the loop early.
      result = FilteringBehavior::kAllow;
      break;
    } else if (result == FilteringBehavior::kBlock) {
      // The current matching allowed entry is a conflict.
      conflict_type = AddConflict(
          conflict_type,
          HostHasTrivialSubdomainConflict(host_entry, blocked_host_list_));
      // If we have already detected all the possible conflict kinds, exit
      // the loop early.
      if (conflict_type == FilteringSubdomainConflictType::
                               kTrivialSubdomainConflictAndOtherConflict) {
        break;
      }
    }
  }
  return result;
}

bool SupervisedUserURLFilter::GetFilteringBehaviorForURLWithAsyncChecks(
    const GURL& url,
    FilteringBehaviorCallback callback,
    bool skip_manual_parent_filter) {
  supervised_user::FilteringBehaviorReason reason =
      supervised_user::FilteringBehaviorReason::DEFAULT;
  FilteringBehavior behavior = GetFilteringBehaviorForURL(url, &reason);

  if (behavior == FilteringBehavior::kAllow &&
      reason != supervised_user::FilteringBehaviorReason::DEFAULT) {
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_) {
      observer.OnURLChecked(url, behavior, {reason});
    }
    return true;
  }

  if (!skip_manual_parent_filter) {
    // Any non-default reason trumps the async checker.
    // Also, if we're blocking anyway, then there's no need to check it.
    if (reason != supervised_user::FilteringBehaviorReason::DEFAULT ||
        behavior == FilteringBehavior::kBlock) {
      std::move(callback).Run(behavior, reason, false);
      for (Observer& observer : observers_) {
        observer.OnURLChecked(url, behavior, {reason});
      }
      return true;
    }
  }

  // Runs mature url filter.
  return RunAsyncChecker(url, std::move(callback));
}

bool SupervisedUserURLFilter::GetFilteringBehaviorForSubFrameURLWithAsyncChecks(
    const GURL& url,
    const GURL& main_frame_url,
    FilteringBehaviorCallback callback) {
  supervised_user::FilteringBehaviorReason reason =
      supervised_user::FilteringBehaviorReason::DEFAULT;
  FilteringBehavior behavior = GetFilteringBehaviorForURL(url, &reason);

  // If the reason is not default, then it is manually allowed or blocked.
  if (reason != supervised_user::FilteringBehaviorReason::DEFAULT) {
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_) {
      observer.OnURLChecked(url, behavior, {reason});
    }
    return true;
  }

  // If the reason is default and behavior is block and the subframe url is not
  // the same domain as the main frame, block the subframe.
  if (behavior == FilteringBehavior::kBlock &&
      !IsSameDomain(url, main_frame_url)) {
    // It is not in the same domain and is blocked.
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_) {
      observer.OnURLChecked(url, behavior, {reason});
    }
    return true;
  }

  // Runs mature url filter.
  return RunAsyncChecker(url, std::move(callback));
}

void SupervisedUserURLFilter::SetDefaultFilteringBehavior(
    FilteringBehavior behavior) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  default_behavior_ = behavior;
}

supervised_user::FilteringBehavior
SupervisedUserURLFilter::GetDefaultFilteringBehavior() const {
  return default_behavior_;
}

void SupervisedUserURLFilter::SetManualHosts(
    std::map<std::string, bool> host_map) {
  // TODO(b/305229682): Update this method to received the two
  // parental lists.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocked_host_list_.clear();
  allowed_host_list_.clear();
  for (const auto& host_entry : host_map) {
    if (host_entry.second) {
      allowed_host_list_.emplace(host_entry.first);
    } else {
      blocked_host_list_.emplace(host_entry.first);
    }
  }
}

bool SupervisedUserURLFilter::IsManualHostsEmpty() const {
  return allowed_host_list_.empty() && blocked_host_list_.empty();
}

void SupervisedUserURLFilter::SetManualURLs(std::map<GURL, bool> url_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_map_ = std::move(url_map);
}

void SupervisedUserURLFilter::Clear() {
  default_behavior_ = FilteringBehavior::kAllow;
  url_map_.clear();
  allowed_host_list_.clear();
  blocked_host_list_.clear();
  async_url_checker_.reset();
  is_filter_initialized_ = false;
}

void SupervisedUserURLFilter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SupervisedUserURLFilter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

WebFilterType SupervisedUserURLFilter::GetWebFilterType() const {
  // If the default filtering behavior is not block, it means the web filter
  // was set to either "allow all sites" or "try to block mature sites".
  if (default_behavior_ == FilteringBehavior::kBlock) {
    return WebFilterType::kCertainSites;
  }

  return supervised_user::IsSafeSitesEnabled(user_prefs_.get())
             ? WebFilterType::kTryToBlockMatureSites
             : WebFilterType::kAllowAllSites;
}

bool SupervisedUserURLFilter::EmitURLFilterMetrics() const {
  // Do not record metrics if the parent web filter configuration is not
  // applied to the user.
  if (!is_filter_initialized_) {
    return false;
  }

  ReportWebFilterTypeMetrics();
  ReportManagedSiteListMetrics();
  return true;
}

void SupervisedUserURLFilter::ReportWebFilterTypeMetrics() const {
  base::UmaHistogramEnumeration(kWebFilterTypeHistogramName,
                                GetWebFilterType());
}

void SupervisedUserURLFilter::ReportManagedSiteListMetrics() const {
  if (url_map_.empty() && allowed_host_list_.empty() &&
      blocked_host_list_.empty()) {
    base::UmaHistogramEnumeration(kManagedSiteListHistogramName,
                                  ManagedSiteList::kEmpty);
    base::UmaHistogramCounts1000(kApprovedSitesCountHistogramName, 0);
    base::UmaHistogramCounts1000(kBlockedSitesCountHistogramName, 0);
    return;
  }

  ManagedSiteList managed_site_list = ManagedSiteList::kMaxValue;
  int approved_count = 0;
  int blocked_count = 0;
  for (const auto& it : url_map_) {
    if (it.second) {
      approved_count++;
    } else {
      blocked_count++;
    }
  }

  approved_count += allowed_host_list_.size();
  blocked_count += blocked_host_list_.size();

  if (approved_count > 0 && blocked_count > 0) {
    managed_site_list = ManagedSiteList::kBoth;
  } else if (approved_count > 0) {
    managed_site_list = ManagedSiteList::kApprovedListOnly;
  } else {
    managed_site_list = ManagedSiteList::kBlockedListOnly;
  }

  base::UmaHistogramCounts1000(kApprovedSitesCountHistogramName,
                               approved_count);
  base::UmaHistogramCounts1000(kBlockedSitesCountHistogramName, blocked_count);

  base::UmaHistogramEnumeration(kManagedSiteListHistogramName,
                                managed_site_list);
}

void SupervisedUserURLFilter::SetFilterInitialized(bool is_filter_initialized) {
  is_filter_initialized_ = is_filter_initialized;
}

bool SupervisedUserURLFilter::RunAsyncChecker(
    const GURL& url,
    FilteringBehaviorCallback callback) const {
  // The parental setting may allow all sites to be visited.
  if (GetWebFilterType() == WebFilterType::kAllowAllSites) {
    std::move(callback).Run(FilteringBehavior::kAllow,
                            supervised_user::FilteringBehaviorReason::DEFAULT,
                            false);
    return true;
  }

  // The primary account must be supervised to run async URL classification.
  CHECK(supervised_user::IsSubjectToParentalControls(user_prefs_.get()));
  CHECK(async_url_checker_);
  return async_url_checker_->CheckURL(
      url_matcher::util::Normalize(url),
      base::BindOnce(&SupervisedUserURLFilter::CheckCallback,
                     base::Unretained(this), std::move(callback)));
}

void SupervisedUserURLFilter::SetURLCheckerClient(
    std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client) {
  async_url_checker_.reset(
      new safe_search_api::URLChecker(std::move(url_checker_client)));
}

bool SupervisedUserURLFilter::IsHostInBlocklist(const std::string& host) const {
  return blocked_host_list_.contains(host);
}

void SupervisedUserURLFilter::CheckCallback(
    FilteringBehaviorCallback callback,
    const GURL& url,
    safe_search_api::Classification classification,
    safe_search_api::ClassificationDetails details) const {
  FilteringBehavior behavior =
      GetBehaviorFromSafeSearchClassification(classification);
  std::move(callback).Run(
      behavior, supervised_user::FilteringBehaviorReason::ASYNC_CHECKER,
      details.reason ==
          safe_search_api::ClassificationDetails::Reason::kFailedUseDefault);
  for (Observer& observer : observers_) {
    observer.OnURLChecked(
        url, behavior,
        supervised_user::FilteringBehaviorDetails{
            .reason = supervised_user::FilteringBehaviorReason::ASYNC_CHECKER,
            .classification_details = details});
  }
}

}  // namespace supervised_user
