// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/google/core/common/google_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/web_contents.h"

namespace {

using ::boca::LockedNavigationOptions;

constexpr char kAllTrafficWildcard[] = "*";

constexpr std::string_view kCommonUrlPrefix = "www.";

// Returns a URL filter that covers all URL navigations.
base::ListValue GetAllTrafficFilter() {
  base::ListValue all_traffic;
  all_traffic.Append(kAllTrafficWildcard);
  return all_traffic;
}

void RemovePrefix(std::string& url_str, std::string_view prefix) {
  if (base::StartsWith(url_str, prefix)) {
    std::string::size_type iter = url_str.find(prefix);
    if (iter != std::string::npos) {
      url_str.erase(iter, prefix.length());
    }
  }
}

base::ListValue GetDomainLevelTrafficFilter(const GURL& url) {
  base::ListValue allowed_traffic;

  std::string domain_traffic_filter = url.GetWithEmptyPath().GetContent();

  RemovePrefix(domain_traffic_filter, kCommonUrlPrefix);
  allowed_traffic.Append(domain_traffic_filter);
  return allowed_traffic;
}

base::ListValue GetLimitedTrafficFilter(const GURL& url) {
  base::ListValue allowed_traffic;
  allowed_traffic.Append("." + url.spec());
  return allowed_traffic;
}
}  // namespace

OnTaskBlocklist::OnTaskBlocklist(
    std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager)
    : url_blocklist_manager_(std::move(url_blocklist_manager)) {}

OnTaskBlocklist::~OnTaskBlocklist() {
  CleanupBlocklist();
}

// static
bool OnTaskBlocklist::IsURLInDomain(const GURL& url, const GURL& domain_url) {
  base::ListValue domain_level_traffic_filter =
      GetDomainLevelTrafficFilter(domain_url);
  url_matcher::URLMatcher url_matcher;
  url_matcher::util::AddAllowFiltersWithLimit(&url_matcher,
                                              domain_level_traffic_filter);
  return !url_matcher.MatchURL(url).empty();
}

policy::URLBlocklist::URLBlocklistState OnTaskBlocklist::GetURLBlocklistState(
    const GURL& url,
    content::WebContents* tab) const {
  LockedNavigationOptions::NavigationType restriction_level =
      GetRestrictionLevelForTab(tab);
  if (restriction_level == LockedNavigationOptions::OPEN_NAVIGATION) {
    return policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
  }

  // Only allow users to navigate within Google domain URLs if the nav
  // restriction is set to `WORKSPACE_NAVIGATION`.
  if (restriction_level == LockedNavigationOptions::WORKSPACE_NAVIGATION) {
    if (google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                       google_util::ALLOW_NON_STANDARD_PORTS)) {
      return policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
    }
    return policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
  }

  // Allow 1LD navigations synchronously at the blocklist level. The navigation
  // throttle performs the actual fine-grained enforcement (like 1LD and
  // same-domain checks). This prevents background tab navigations from being
  // incorrectly blocked based on the nav restrictions enforced on the active or
  // foreground tab.
  if (IsTabRestrictionOneLevelDeep(tab)) {
    return policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
  }

  // Evaluate domain restrictions synchronously to account for background tab
  // navigations. We cannot rely on the blocklist override since those are only
  // refreshed on tab activation and are based on the nav restrictions enforced
  // on the foreground tab.
  const GURL tab_original_url = GetOriginalURLForTab(tab);
  if (tab_original_url.is_valid() &&
      restriction_level == LockedNavigationOptions::DOMAIN_NAVIGATION) {
    return IsURLInDomain(url, tab_original_url)
               ? policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST
               : policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
  }

  if (tab_original_url.is_valid() &&
      restriction_level == LockedNavigationOptions::BLOCK_NAVIGATION) {
    return tab_original_url == url
               ? policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST
               : policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
  }

  return url_blocklist_manager_->GetURLBlocklistState(url);
}

bool OnTaskBlocklist::IsTabRestrictionOneLevelDeep(
    content::WebContents* tab) const {
  LockedNavigationOptions::NavigationType restriction_level =
      GetRestrictionLevelForTab(tab);
  return (restriction_level == LockedNavigationOptions::LIMITED_NAVIGATION ||
          restriction_level ==
              LockedNavigationOptions::
                  SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION);
}

bool OnTaskBlocklist::MaybeSetURLRestrictionLevel(
    content::WebContents* tab,
    const GURL& url,
    LockedNavigationOptions::NavigationType restriction_level) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return false;
  }

  // Don't let unintended update of restrictions level for tabs.
  if (parent_tab_to_nav_filters_.contains(tab_id) ||
      child_tab_to_nav_filters_.contains(tab_id)) {
    return false;
  } else {
    child_tab_to_nav_filters_[tab_id] = restriction_level;
  }

  tab_to_original_url_[tab_id] = url;

  if (restriction_level == LockedNavigationOptions::LIMITED_NAVIGATION ||
      restriction_level ==
          LockedNavigationOptions::
              SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION) {
    one_level_deep_original_url_[tab_id] = url;
  }
  return true;
}

void OnTaskBlocklist::SetParentURLRestrictionLevel(
    content::WebContents* tab,
    const GURL& url,
    LockedNavigationOptions::NavigationType restriction_level) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return;
  }
  parent_tab_to_nav_filters_[tab_id] = restriction_level;
  tab_to_original_url_[tab_id] = url;

  if (restriction_level == LockedNavigationOptions::LIMITED_NAVIGATION ||
      restriction_level ==
          LockedNavigationOptions::
              SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION) {
    one_level_deep_original_url_[tab_id] = url;
  }
}

void OnTaskBlocklist::RefreshForUrlBlocklist(content::WebContents* tab) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return;
  }

  const GURL& url = tab->GetVisibleURL();
  if (previous_tab_id_ == tab_id && previous_url_.is_valid() &&
      previous_url_ == url) {
    return;
  }

  LockedNavigationOptions::NavigationType restriction_level =
      GetRestrictionLevelForTab(tab);

  // Fallback checks for url redirect opening in a new tab.
  if (!child_tab_to_nav_filters_.contains(tab_id) &&
      !parent_tab_to_nav_filters_.contains(tab_id)) {
    if (restriction_level == LockedNavigationOptions::LIMITED_NAVIGATION) {
      restriction_level = LockedNavigationOptions::BLOCK_NAVIGATION;
    } else if (restriction_level ==
               LockedNavigationOptions::
                   SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION) {
      if (!IsURLInDomain(url, GetOriginalURLForTab(tab))) {
        restriction_level = LockedNavigationOptions::BLOCK_NAVIGATION;
      }
    }
  }

  previous_url_ = url;
  previous_tab_id_ = tab_id;
  url_blocklist_manager_->SetOverrideBlockListSource(
      std::make_unique<OnTaskBlocklistSource>(url, restriction_level));
}

void OnTaskBlocklist::RemoveParentFilter(content::WebContents* tab) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (tab_id.is_valid()) {
    parent_tab_to_nav_filters_.erase(tab_id);
    tab_to_original_url_.erase(tab_id);
  }
}

void OnTaskBlocklist::RemoveChildFilter(content::WebContents* tab) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (tab_id.is_valid()) {
    child_tab_to_nav_filters_.erase(tab_id);
    tab_to_original_url_.erase(tab_id);
    child_to_parent_tab_id_.erase(tab_id);
  }
}

bool OnTaskBlocklist::CanPerformOneLevelNavigation(
    content::WebContents* tab) const {
  // This method should only be called if the current restriction level is set
  // to either `kOneLevelDeepNavigation` or `kDomainAndOneLevelDeepNavigation`.
  CHECK(IsTabRestrictionOneLevelDeep(tab));

  if (!tab) {
    return false;
  }

  // For one level deep (1LD) navigation restriction, we check if the last
  // committed URL is the same as the original URL being tracked. This helps us
  // determine if we have already navigated 1LD.
  //
  // For same domain + 1LD navigation restriction, we check if the last
  // committed URL is in the same domain as the original URL that was being
  // tracked. This helps us determine if we have already navigated 1LD.
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (tab_id.is_valid()) {
    if (auto it = one_level_deep_original_url_.find(tab_id);
        it != one_level_deep_original_url_.end()) {
      const GURL one_level_deep_original_url = it->second;
      const GURL last_committed_url = tab->GetLastCommittedURL();
      if (GetRestrictionLevelForTab(tab) ==
          LockedNavigationOptions::LIMITED_NAVIGATION) {
        return one_level_deep_original_url == last_committed_url;
      }

      // Same domain + 1LD navigation restriction.
      return last_committed_url.is_valid() &&
             IsURLInDomain(last_committed_url, one_level_deep_original_url);
    }
  }
  return true;
}

bool OnTaskBlocklist::IsParentTab(content::WebContents* tab) const {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return false;
  }

  return parent_tab_to_nav_filters_.contains(tab_id);
}

const policy::URLBlocklistManager* OnTaskBlocklist::url_blocklist_manager() {
  return url_blocklist_manager_.get();
}

std::map<SessionID, LockedNavigationOptions::NavigationType>
OnTaskBlocklist::parent_tab_to_nav_filters() const {
  return parent_tab_to_nav_filters_;
}

std::map<SessionID, LockedNavigationOptions::NavigationType>
OnTaskBlocklist::child_tab_to_nav_filters() const {
  return child_tab_to_nav_filters_;
}

void OnTaskBlocklist::CleanupBlocklist() {
  url_blocklist_manager_->SetOverrideBlockListSource(nullptr);
  parent_tab_to_nav_filters_.clear();
  child_tab_to_nav_filters_.clear();
  one_level_deep_original_url_.clear();
  tab_to_original_url_.clear();
  child_to_parent_tab_id_.clear();
  previous_tab_id_ = SessionID::InvalidValue();
}

LockedNavigationOptions::NavigationType
OnTaskBlocklist::GetRestrictionLevelForTab(content::WebContents* tab) const {
  if (!tab) {
    return LockedNavigationOptions::OPEN_NAVIGATION;
  }
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return LockedNavigationOptions::OPEN_NAVIGATION;
  }
  if (auto it = child_tab_to_nav_filters_.find(tab_id);
      it != child_tab_to_nav_filters_.end()) {
    return it->second;
  }
  if (auto it = parent_tab_to_nav_filters_.find(tab_id);
      it != parent_tab_to_nav_filters_.end()) {
    return it->second;
  }

  // Fallback checks for newly spawned child tabs.
  const SessionID parent_tab_id = GetParentTabId(tab);
  if (parent_tab_id.is_valid()) {
    if (auto it = child_tab_to_nav_filters_.find(parent_tab_id);
        it != child_tab_to_nav_filters_.end()) {
      return it->second;
    }
    if (auto it = parent_tab_to_nav_filters_.find(parent_tab_id);
        it != parent_tab_to_nav_filters_.end()) {
      return it->second;
    }
  }
  return LockedNavigationOptions::OPEN_NAVIGATION;
}

GURL OnTaskBlocklist::GetOriginalURLForTab(content::WebContents* tab) const {
  if (!tab) {
    return GURL();
  }
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return GURL();
  }
  if (auto it = tab_to_original_url_.find(tab_id);
      it != tab_to_original_url_.end()) {
    return it->second;
  }

  // Fallback checks for newly spawned child tabs.
  const SessionID parent_tab_id = GetParentTabId(tab);
  if (parent_tab_id.is_valid() &&
      tab_to_original_url_.contains(parent_tab_id)) {
    return tab_to_original_url_.at(parent_tab_id);
  }
  return GURL();
}

void OnTaskBlocklist::SetParentForTab(content::WebContents* child_tab,
                                      content::WebContents* parent_tab) {
  if (!child_tab || !parent_tab) {
    return;
  }
  const SessionID child_tab_id =
      sessions::SessionTabHelper::IdForTab(child_tab);
  const SessionID parent_tab_id =
      sessions::SessionTabHelper::IdForTab(parent_tab);
  if (child_tab_id.is_valid() && parent_tab_id.is_valid()) {
    child_to_parent_tab_id_.insert_or_assign(child_tab_id, parent_tab_id);
  }
}

SessionID OnTaskBlocklist::GetParentTabId(content::WebContents* tab) const {
  if (!tab) {
    return SessionID::InvalidValue();
  }
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return SessionID::InvalidValue();
  }
  if (auto it = child_to_parent_tab_id_.find(tab_id);
      it != child_to_parent_tab_id_.end()) {
    return it->second;
  }
  return SessionID::InvalidValue();
}

GURL OnTaskBlocklist::GetOneLevelDeepOriginalURL(SessionID tab_id) const {
  if (!tab_id.is_valid()) {
    return GURL();
  }
  auto it = one_level_deep_original_url_.find(tab_id);
  if (it == one_level_deep_original_url_.end()) {
    return GURL();
  }
  return it->second;
}

// OnTaskBlock::BlocklistSource Implementation
OnTaskBlocklist::OnTaskBlocklistSource::OnTaskBlocklistSource(
    const GURL& url,
    LockedNavigationOptions::NavigationType restriction_type) {
  switch (restriction_type) {
    case LockedNavigationOptions::
        SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION:
    case LockedNavigationOptions::LIMITED_NAVIGATION:
    case LockedNavigationOptions::OPEN_NAVIGATION:
      allowlist_ = GetAllTrafficFilter();
      return;
    case LockedNavigationOptions::DOMAIN_NAVIGATION:
      blocklist_ = GetAllTrafficFilter();
      allowlist_ = GetDomainLevelTrafficFilter(url);
      return;
    case LockedNavigationOptions::NAVIGATION_TYPE_UNKNOWN:
    case LockedNavigationOptions::BLOCK_NAVIGATION:
      blocklist_ = GetAllTrafficFilter();
      allowlist_ = GetLimitedTrafficFilter(url);
      return;
    default:
      blocklist_ = GetAllTrafficFilter();
      allowlist_ = GetLimitedTrafficFilter(url);
      return;
  }
}

const base::ListValue*
OnTaskBlocklist::OnTaskBlocklistSource::GetBlocklistSpec() const {
  return &blocklist_;
}

const base::ListValue*
OnTaskBlocklist::OnTaskBlocklistSource::GetAllowlistSpec() const {
  return &allowlist_;
}

bool OnTaskBlocklist::OnTaskBlocklistSource::
    DowngradeAllowlistWildcardToNeutral() const {
  return false;
}
