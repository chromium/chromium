// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
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

const std::string& GetCommonUrlPrefix() {
  static const base::NoDestructor<std::string> prefix("www.");
  return *prefix;  // provides pointer-like access
}

// Returns a URL filter that covers all URL navigations.
base::Value::List GetAllTrafficFilter() {
  base::Value::List all_traffic;
  all_traffic.Append(kAllTrafficWildcard);
  return all_traffic;
}

void RemovePrefix(std::string& url_str, const std::string& prefix) {
  if (base::StartsWith(url_str, prefix)) {
    std::string::size_type iter = url_str.find(prefix);
    if (iter != std::string::npos) {
      url_str.erase(iter, prefix.length());
    }
  }
}

base::Value::List GetDomainLevelTrafficFilter(const GURL& url) {
  base::Value::List allowed_traffic;

  std::string domain_traffic_filter = url.GetWithEmptyPath().GetContent();

  RemovePrefix(domain_traffic_filter, GetCommonUrlPrefix());
  allowed_traffic.Append(domain_traffic_filter);
  return allowed_traffic;
}

base::Value::List GetLimitedTrafficFilter(const GURL& url) {
  base::Value::List allowed_traffic;
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
  base::Value::List domain_level_traffic_filter =
      GetDomainLevelTrafficFilter(domain_url);
  url_matcher::URLMatcher url_matcher;
  url_matcher::util::AddAllowFiltersWithLimit(&url_matcher,
                                              domain_level_traffic_filter);
  return !url_matcher.MatchURL(url).empty();
}

policy::URLBlocklist::URLBlocklistState OnTaskBlocklist::GetURLBlocklistState(
    const GURL& url) const {
  if (current_page_restriction_level_ ==
      LockedNavigationOptions::OPEN_NAVIGATION) {
    return policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
  }

  // Only allow users to navigate within Google domain URLs if the nav
  // restriction is set to `WORKSPACE_NAVIGATION`.
  if (current_page_restriction_level_ ==
      LockedNavigationOptions::WORKSPACE_NAVIGATION) {
    if (google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                       google_util::ALLOW_NON_STANDARD_PORTS)) {
      return policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
    }
    return policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
  }

  if (previous_url_.is_valid() &&
      current_page_restriction_level_ ==
          LockedNavigationOptions::BLOCK_NAVIGATION) {
    return previous_url_ == url
               ? policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST
               : policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
  }
  return url_blocklist_manager_->GetURLBlocklistState(url);
}

bool OnTaskBlocklist::IsCurrentRestrictionOneLevelDeep() {
  return (current_page_restriction_level_ ==
              LockedNavigationOptions::LIMITED_NAVIGATION ||
          current_page_restriction_level_ ==
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
  if (base::Contains(parent_tab_to_nav_filters_, tab_id) ||
      base::Contains(child_tab_to_nav_filters_, tab_id)) {
    return false;
  } else {
    child_tab_to_nav_filters_[tab_id] = restriction_level;
  }
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
  // `previous_tab_` should only be not valid when we first navigate to the
  // first tab when the OnTask SWA is first launched. Every other instance
  // should have a valid `previous_tab_`.
  if (previous_tab() && previous_tab() == tab && previous_url_.is_valid() &&
      previous_url_ == url) {
    return;
  }

  std::unique_ptr<OnTaskBlocklistSource> blocklist_source;
  LockedNavigationOptions::NavigationType restriction_level;
  // Updates the blocklist given the active tab's url. This function does a
  // series of checks to determine what restriction levels apply. It starts at
  // closest match starting from the child maps and continues outwards to least
  // restrictive url matching in case urls have been redirected or have its url
  // rewritten (ex. google drive home page to user authenticated google drive
  // home page.). Note: The navigation throttler is responsible for updating the
  // web contents and their restriction levels.
  if (base::Contains(child_tab_to_nav_filters_, tab_id)) {
    restriction_level = child_tab_to_nav_filters_[tab_id];
    blocklist_source =
        std::make_unique<OnTaskBlocklistSource>(url, restriction_level);
    current_page_restriction_level_ = restriction_level;

  } else if (base::Contains(parent_tab_to_nav_filters_, tab_id)) {
    restriction_level = parent_tab_to_nav_filters_[tab_id];
    blocklist_source =
        std::make_unique<OnTaskBlocklistSource>(url, restriction_level);
    current_page_restriction_level_ = restriction_level;
  } else {
    // Should only happen if a url redirect opens in a new tab.
    if (current_page_restriction_level_ ==
        LockedNavigationOptions::LIMITED_NAVIGATION) {
      blocklist_source = std::make_unique<OnTaskBlocklistSource>(
          url, LockedNavigationOptions::BLOCK_NAVIGATION);
      current_page_restriction_level_ =
          LockedNavigationOptions::BLOCK_NAVIGATION;
    } else if (current_page_restriction_level_ ==
               LockedNavigationOptions::
                   SAME_DOMAIN_OPEN_OTHER_DOMAIN_LIMITED_NAVIGATION) {
      if (!IsURLInDomain(url, previous_url_)) {
        blocklist_source = std::make_unique<OnTaskBlocklistSource>(
            url, LockedNavigationOptions::DOMAIN_NAVIGATION);
        current_page_restriction_level_ =
            LockedNavigationOptions::BLOCK_NAVIGATION;
      }
    } else {
      blocklist_source = std::make_unique<OnTaskBlocklistSource>(
          url, current_page_restriction_level_);
    }
  }

  previous_url_ = url;
  previous_tab_ = tab->GetWeakPtr();
  url_blocklist_manager_->SetOverrideBlockListSource(
      std::move(blocklist_source));
}

void OnTaskBlocklist::RemoveParentFilter(content::WebContents* tab) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (tab_id.is_valid() && base::Contains(parent_tab_to_nav_filters_, tab_id)) {
    parent_tab_to_nav_filters_.erase(tab_id);
  }
}

void OnTaskBlocklist::RemoveChildFilter(content::WebContents* tab) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (tab_id.is_valid() && base::Contains(child_tab_to_nav_filters_, tab_id)) {
    child_tab_to_nav_filters_.erase(tab_id);
  }
}

bool OnTaskBlocklist::CanPerformOneLevelNavigation(content::WebContents* tab) {
  // This method should only be called if the current restriction level is set
  // to either `kOneLevelDeepNavigation` or `kDomainAndOneLevelDeepNavigation`.
  CHECK(IsCurrentRestrictionOneLevelDeep());

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
  if (tab_id.is_valid() &&
      base::Contains(one_level_deep_original_url_, tab_id)) {
    const GURL one_level_deep_original_url =
        one_level_deep_original_url_[tab_id];
    const GURL last_committed_url = tab->GetLastCommittedURL();
    if (current_page_restriction_level_ ==
        LockedNavigationOptions::LIMITED_NAVIGATION) {
      return one_level_deep_original_url == last_committed_url;
    }

    // Same domain + 1LD navigation restriction.
    return last_committed_url.is_valid() &&
           IsURLInDomain(last_committed_url, one_level_deep_original_url);
  }
  return true;
}

bool OnTaskBlocklist::IsParentTab(content::WebContents* tab) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return false;
  }

  return base::Contains(parent_tab_to_nav_filters_, tab_id);
}

const policy::URLBlocklistManager* OnTaskBlocklist::url_blocklist_manager() {
  return url_blocklist_manager_.get();
}

std::map<SessionID, LockedNavigationOptions::NavigationType>
OnTaskBlocklist::parent_tab_to_nav_filters() {
  return parent_tab_to_nav_filters_;
}

std::map<SessionID, LockedNavigationOptions::NavigationType>
OnTaskBlocklist::child_tab_to_nav_filters() {
  return child_tab_to_nav_filters_;
}

std::map<SessionID, GURL> OnTaskBlocklist::one_level_deep_original_url() {
  return one_level_deep_original_url_;
}

LockedNavigationOptions::NavigationType
OnTaskBlocklist::current_page_restriction_level() {
  return current_page_restriction_level_;
}

content::WebContents* OnTaskBlocklist::previous_tab() {
  if (!previous_tab_) {
    return nullptr;
  }
  return previous_tab_.get();
}

void OnTaskBlocklist::CleanupBlocklist() {
  url_blocklist_manager_->SetOverrideBlockListSource(nullptr);
  parent_tab_to_nav_filters_.clear();
  child_tab_to_nav_filters_.clear();
  one_level_deep_original_url_.clear();
  previous_tab_ = nullptr;
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

const base::Value::List*
OnTaskBlocklist::OnTaskBlocklistSource::GetBlocklistSpec() const {
  return &blocklist_;
}

const base::Value::List*
OnTaskBlocklist::OnTaskBlocklistSource::GetAllowlistSpec() const {
  return &allowlist_;
}
