// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filter.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/kids_management_url_checker_client.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/core/common/supervised_user_utils.h"
#include "components/url_matcher/url_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
using net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using net::registry_controlled_domains::GetCanonicalHostRegistryLength;

namespace supervised_user {

SupervisedUserURLFilter::FilteringBehavior
GetBehaviorFromSafeSearchClassification(
    safe_search_api::Classification classification) {
  switch (classification) {
    case safe_search_api::Classification::SAFE:
      return SupervisedUserURLFilter::ALLOW;
    case safe_search_api::Classification::UNSAFE:
      return SupervisedUserURLFilter::BLOCK;
  }
  NOTREACHED();
  return SupervisedUserURLFilter::BLOCK;
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
  constexpr auto kAllowedHosts = base::MakeFixedFlatSet<base::StringPiece>(
      {"accounts.google.com", "families.google.com", "familylink.google.com",
       "myaccount.google.com", "policies.google.com", "support.google.com"});

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
  static const char* kPlayStoreHost = "play.google.com";
  static const char* kPlayTermsPath = "/about/play-terms";
  // Check Play Store terms of service.
  // path_piece is checked separately from the host to match international pages
  // like https://play.google.com/intl/pt-BR_pt/about/play-terms/.
  return effective_url.SchemeIs(url::kHttpsScheme) &&
         effective_url.host_piece() == kPlayStoreHost &&
         (effective_url.path_piece().find(kPlayTermsPath) !=
          base::StringPiece::npos);
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
    ValidateURLSupportCallback check_webstore_url_callback,
    std::unique_ptr<Delegate> service_delegate)
    : default_behavior_(ALLOW),
      service_delegate_(std::move(service_delegate)),
      blocking_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      check_webstore_url_callback_(std::move(check_webstore_url_callback)) {}

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
SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::BehaviorFromInt(int behavior_value) {
  DCHECK(behavior_value == ALLOW || behavior_value == BLOCK)
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

// static
std::string SupervisedUserURLFilter::WebFilterTypeToDisplayString(
    WebFilterType web_filter_type) {
  switch (web_filter_type) {
    case WebFilterType::kAllowAllSites:
      return "allow_all_sites";
    case WebFilterType::kCertainSites:
      return "allow_certain_sites";
    case WebFilterType::kTryToBlockMatureSites:
      return "block_mature_sites";
  }
}

SupervisedUserFilterTopLevelResult
SupervisedUserURLFilter::GetHistogramValueForTopLevelFilteringBehavior(
    FilteringBehavior behavior,
    FilteringBehaviorReason reason,
    bool is_filtering_behavior_known) {
  switch (behavior) {
    case ALLOW:
      return SupervisedUserFilterTopLevelResult::kAllow;
    case BLOCK:
      switch (reason) {
        case FilteringBehaviorReason::ASYNC_CHECKER:
          return SupervisedUserFilterTopLevelResult::kBlockSafeSites;
        case FilteringBehaviorReason::ALLOWLIST:
          NOTREACHED();
          break;
        case FilteringBehaviorReason::MANUAL:
          return SupervisedUserFilterTopLevelResult::kBlockManual;
        case FilteringBehaviorReason::DEFAULT:
          return SupervisedUserFilterTopLevelResult::kBlockNotInAllowlist;
        case FilteringBehaviorReason::NOT_SIGNED_IN:
          NOTREACHED();
      }
      [[fallthrough]];
    case INVALID:
      NOTREACHED();
  }
  NOTREACHED();
  return SupervisedUserFilterTopLevelResult::kAllow;
}

// static
int SupervisedUserURLFilter::GetHistogramValueForFilteringBehavior(
    FilteringBehavior behavior,
    FilteringBehaviorReason reason,
    bool is_filtering_behavior_known) {
  switch (behavior) {
    case ALLOW:
      if (reason == FilteringBehaviorReason::ALLOWLIST) {
        return SupervisedUserSafetyFilterResult::
            FILTERING_BEHAVIOR_ALLOW_ALLOWLIST;
      }
      return is_filtering_behavior_known
                 ? SupervisedUserSafetyFilterResult::FILTERING_BEHAVIOR_ALLOW
                 : SupervisedUserSafetyFilterResult::
                       FILTERING_BEHAVIOR_ALLOW_UNCERTAIN;
    case BLOCK:
      switch (reason) {
        case FilteringBehaviorReason::ASYNC_CHECKER:
          return SupervisedUserSafetyFilterResult::
              FILTERING_BEHAVIOR_BLOCK_SAFESITES;
        case FilteringBehaviorReason::ALLOWLIST:
          NOTREACHED();
          break;
        case FilteringBehaviorReason::MANUAL:
          return SupervisedUserSafetyFilterResult::
              FILTERING_BEHAVIOR_BLOCK_MANUAL;
        case FilteringBehaviorReason::DEFAULT:
          return SupervisedUserSafetyFilterResult::
              FILTERING_BEHAVIOR_BLOCK_DEFAULT;
        case FilteringBehaviorReason::NOT_SIGNED_IN:
          // Should never happen, only used for requests from Webview
          NOTREACHED();
      }
      [[fallthrough]];
    case INVALID:
      NOTREACHED();
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

SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetFilteringBehaviorForURL(const GURL& url) {
  supervised_user::FilteringBehaviorReason reason;
  return GetFilteringBehaviorForURL(url, &reason);
}

bool SupervisedUserURLFilter::IsExemptedFromGuardianApproval(
    const GURL& effective_url) {
  DCHECK(!check_webstore_url_callback_.is_null());
  return IsNonStandardUrlScheme(effective_url) ||
         IsAlwaysAllowedHost(effective_url) ||
         IsAlwaysAllowedUrlPrefix(effective_url) ||
         IsPlayStoreTermsOfServiceUrl(effective_url) ||
         check_webstore_url_callback_.Run(effective_url);
}

bool SupervisedUserURLFilter::GetManualFilteringBehaviorForURL(
    const GURL& url,
    FilteringBehavior* behavior) {
  supervised_user::FilteringBehaviorReason reason;
  *behavior = GetFilteringBehaviorForURL(url, &reason);
  return reason == supervised_user::FilteringBehaviorReason::MANUAL;
}

SupervisedUserURLFilter::FilteringBehavior
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
    return ALLOW;
  }

  // Check manual denylists and allowlists.
  FilteringBehavior manual_result =
      GetManualFilteringBehaviorForURL(effective_url);
  if (manual_result != INVALID) {
    return manual_result;
  }

  // Fall back to the default behavior.
  *reason = supervised_user::FilteringBehaviorReason::DEFAULT;
  return default_behavior_;
}

// There may be conflicting patterns, say, "allow *.google.com" and "block
// www.google.*". To break the tie, we prefer blocklists over allowlists.
// If there are no applicable manual overrides, we return INVALID.
SupervisedUserURLFilter::FilteringBehavior
SupervisedUserURLFilter::GetManualFilteringBehaviorForURL(const GURL& url) {
  FilteringBehavior result = INVALID;
  std::optional<FilteringSubdomainConflictType> conflict_type = std::nullopt;

  // Records the conflict metrics when the current scope exits.
  base::ScopedClosureRunner histogram_recorder(base::BindOnce(
      [](const FilteringBehavior& result,
         const std::optional<FilteringSubdomainConflictType>& conflict_type) {
        if (result != INVALID) {
          // Record the potential conflict and its type.
          bool conflict = conflict_type.has_value();
          UMA_HISTOGRAM_BOOLEAN(kManagedSiteListConflictHistogramName,
                                conflict);
          if (conflict) {
            base::UmaHistogramEnumeration(
                kManagedSiteListSubdomainConflictTypeHistogramName,
                conflict_type.value());
          }
        }
      },
      std::cref(result), std::cref(conflict_type)));

  // Check manual overrides for the exact URL.
  auto url_it = url_map_.find(url_matcher::util::Normalize(url));
  if (url_it != url_map_.end()) {
    result = url_it->second ? ALLOW : BLOCK;
  }

  const std::string host = url.host();
  if (result != BLOCK) {
    // If there is a match with Block behaviour, set the result to Block.
    auto it = base::ranges::find_if(
        blocked_host_list_, [&host](const std::string& host_entry) {
          return HostMatchesPattern(host, host_entry);
        });
    if (it != blocked_host_list_.end()) {
      result = BLOCK;
    }
  }

  if (result == ALLOW) {
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
    if (result == INVALID) {
      // If the result is still unset, there are no conflicts from the
      // blocklist. Set the result and exit the loop early.
      result = ALLOW;
      break;
    } else if (result == BLOCK) {
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

  if (behavior == ALLOW &&
      reason != supervised_user::FilteringBehaviorReason::DEFAULT) {
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_) {
      observer.OnURLChecked(url, behavior, reason, false);
    }
    return true;
  }

  if (!skip_manual_parent_filter) {
    // Any non-default reason trumps the async checker.
    // Also, if we're blocking anyway, then there's no need to check it.
    if (reason != supervised_user::FilteringBehaviorReason::DEFAULT ||
        behavior == BLOCK || !async_url_checker_) {
      std::move(callback).Run(behavior, reason, false);
      for (Observer& observer : observers_) {
        observer.OnURLChecked(url, behavior, reason, false);
      }
      return true;
    }
  }

  // Runs mature url filter if the |async_url_checker_| exists.
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
      observer.OnURLChecked(url, behavior, reason, false);
    }
    return true;
  }

  // If the reason is default and behavior is block and the subframe url is not
  // the same domain as the main frame, block the subframe.
  if (behavior == FilteringBehavior::BLOCK &&
      !IsSameDomain(url, main_frame_url)) {
    // It is not in the same domain and is blocked.
    std::move(callback).Run(behavior, reason, false);
    for (Observer& observer : observers_) {
      observer.OnURLChecked(url, behavior, reason, false);
    }
    return true;
  }

  // Runs mature url filter if the |async_url_checker_| exists.
  return RunAsyncChecker(url, std::move(callback));
}

void SupervisedUserURLFilter::SetDefaultFilteringBehavior(
    FilteringBehavior behavior) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  default_behavior_ = behavior;
}

SupervisedUserURLFilter::FilteringBehavior
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

void SupervisedUserURLFilter::SetManualURLs(std::map<GURL, bool> url_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_map_ = std::move(url_map);
}

void SupervisedUserURLFilter::InitAsyncURLChecker(
    KidsChromeManagementClient* kids_chrome_management_client) {
  DCHECK(service_delegate_);
  std::string country = service_delegate_->GetCountryCode();

  std::unique_ptr<safe_search_api::URLCheckerClient> url_checker_client =
      std::make_unique<KidsManagementURLCheckerClient>(
          kids_chrome_management_client, country);
  async_url_checker_ = std::make_unique<safe_search_api::URLChecker>(
      std::move(url_checker_client));
}

void SupervisedUserURLFilter::ClearAsyncURLChecker() {
  async_url_checker_.reset();
}

bool SupervisedUserURLFilter::HasAsyncURLChecker() const {
  return !!async_url_checker_;
}

void SupervisedUserURLFilter::Clear() {
  default_behavior_ = ALLOW;
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

void SupervisedUserURLFilter::SetBlockingTaskRunnerForTesting(
    const scoped_refptr<base::TaskRunner>& task_runner) {
  blocking_task_runner_ = task_runner;
}

SupervisedUserURLFilter::WebFilterType
SupervisedUserURLFilter::GetWebFilterType() const {
  // If the default filtering behavior is not block, it means the web filter
  // was set to either "allow all sites" or "try to block mature sites".
  if (default_behavior_ == BLOCK) {
    return WebFilterType::kCertainSites;
  }

  bool safe_sites_enabled = HasAsyncURLChecker();
  return safe_sites_enabled ? WebFilterType::kTryToBlockMatureSites
                            : WebFilterType::kAllowAllSites;
}

void SupervisedUserURLFilter::ReportWebFilterTypeMetrics() const {
  if (!is_filter_initialized_) {
    return;
  }

  base::UmaHistogramEnumeration(kWebFilterTypeHistogramName,
                                GetWebFilterType());
}

void SupervisedUserURLFilter::ReportManagedSiteListMetrics() const {
  if (!is_filter_initialized_) {
    return;
  }

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
  // The parental setting may allow all sites to be visited. In such case, the
  // |async_url_checker_| will not be created.
  if (!async_url_checker_) {
    std::move(callback).Run(FilteringBehavior::ALLOW,
                            supervised_user::FilteringBehaviorReason::DEFAULT,
                            false);
    return true;
  }

  return async_url_checker_->CheckURL(
      url_matcher::util::Normalize(url),
      base::BindOnce(&SupervisedUserURLFilter::CheckCallback,
                     base::Unretained(this), std::move(callback)));
}

void SupervisedUserURLFilter::CheckCallback(
    FilteringBehaviorCallback callback,
    const GURL& url,
    safe_search_api::Classification classification,
    bool uncertain) const {
  FilteringBehavior behavior =
      GetBehaviorFromSafeSearchClassification(classification);
  std::move(callback).Run(
      behavior, supervised_user::FilteringBehaviorReason::ASYNC_CHECKER,
      uncertain);
  for (Observer& observer : observers_) {
    observer.OnURLChecked(
        url, behavior, supervised_user::FilteringBehaviorReason::ASYNC_CHECKER,
        uncertain);
  }
}

}  // namespace supervised_user
