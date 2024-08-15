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

namespace {
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
  std::string domain_traffic_filter = "." + url.spec();
  allowed_traffic.Append(domain_traffic_filter);
  return allowed_traffic;
}
}  // namespace

OnTaskBlocklist::OnTaskBlocklist(
    std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager)
    : url_blocklist_manager_(std::move(url_blocklist_manager)) {}

OnTaskBlocklist::~OnTaskBlocklist() {
  CleanupBlocklist();
}

policy::URLBlocklist::URLBlocklistState OnTaskBlocklist::GetURLBlocklistState(
    const GURL& url) const {
  // Enable google domain urls to be allowed to navigated to as long as we were
  // on a google domain. This is especially to allow users to be able to
  // navigate to other areas of google classroom or google drive files. This is
  // only for chromeos specific use case with the OnTask app. The primary use
  // case for the OnTask app is for managed chromebooks under the Edu licenses
  // where they are expected to be Google Workspace users. We should allow
  // traversing various google workspace domains so that the intended integrated
  // workflow for Google Workspace is effective. All other use cases outside
  // of the primary use case will not go through this code path since they have
  // requirements for specific navigation rules set.
  if (google_util::IsGoogleDomainUrl(previous_url_,
                                     google_util::ALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS)) {
    if (google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                       google_util::ALLOW_NON_STANDARD_PORTS) &&
        !google_util::HasGoogleSearchQueryParam(url.query_piece())) {
      return policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
    }
  }
  return url_blocklist_manager_->GetURLBlocklistState(url);
}

void OnTaskBlocklist::SetURLRestrictionLevel(
    content::WebContents* tab,
    OnTaskBlocklist::RestrictionLevel restriction_level) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return;
  }
  if (base::Contains(parent_tab_to_nav_filters_, tab_id)) {
    parent_tab_to_nav_filters_[tab_id] = restriction_level;
  } else {
    child_tab_to_nav_filters_[tab_id] = restriction_level;
  }
  if (restriction_level ==
          OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation ||
      restriction_level ==
          OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation) {
    has_performed_one_level_deep_[tab_id] = false;
  }
}

void OnTaskBlocklist::SetParentURLRestrictionLevel(
    content::WebContents* tab,
    OnTaskBlocklist::RestrictionLevel restriction_level) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return;
  }
  parent_tab_to_nav_filters_[tab_id] = restriction_level;
  if (restriction_level ==
          OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation ||
      restriction_level ==
          OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation) {
    has_performed_one_level_deep_[tab_id] = false;
  }
}

void OnTaskBlocklist::RefreshForUrlBlocklist(content::WebContents* tab) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (!tab_id.is_valid()) {
    return;
  }

  const GURL& url = tab->GetVisibleURL();
  // `previous_url_` should only be not valid when we first navigate to the
  // first tab when the OnTask SWA is first launched. Every other instance
  // should have a valid `previous_url_`.
  if (previous_url_.is_valid() && url.is_valid() && previous_url_ == url) {
    return;
  }

  std::unique_ptr<OnTaskBlocklistSource> blocklist_source;
  OnTaskBlocklist::RestrictionLevel restriction_level;
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
        OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation) {
      blocklist_source = std::make_unique<OnTaskBlocklistSource>(
          url, OnTaskBlocklist::RestrictionLevel::kLimitedNavigation);
      current_page_restriction_level_ =
          OnTaskBlocklist::RestrictionLevel::kLimitedNavigation;
    } else if (current_page_restriction_level_ ==
               OnTaskBlocklist::RestrictionLevel::
                   kDomainAndOneLevelDeepNavigation) {
      if (!url.DomainIs(previous_url_.GetWithEmptyPath().GetContentPiece())) {
        blocklist_source = std::make_unique<OnTaskBlocklistSource>(
            url, OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation);
        current_page_restriction_level_ =
            OnTaskBlocklist::RestrictionLevel::kLimitedNavigation;
      }
    } else {
      blocklist_source = std::make_unique<OnTaskBlocklistSource>(
          url, current_page_restriction_level_);
    }
  }

  previous_url_ = url;
  url_blocklist_manager_->SetOverrideBlockListSource(
      std::move(blocklist_source));
}

void OnTaskBlocklist::RemoveChildFilter(content::WebContents* tab) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab);
  if (tab_id.is_valid() && base::Contains(child_tab_to_nav_filters_, tab_id)) {
    child_tab_to_nav_filters_.erase(tab_id);
  }
}

const policy::URLBlocklistManager* OnTaskBlocklist::url_blocklist_manager() {
  return url_blocklist_manager_.get();
}

std::map<SessionID, OnTaskBlocklist::RestrictionLevel>
OnTaskBlocklist::parent_tab_to_nav_filters() {
  return parent_tab_to_nav_filters_;
}

std::map<SessionID, OnTaskBlocklist::RestrictionLevel>
OnTaskBlocklist::child_tab_to_nav_filters() {
  return child_tab_to_nav_filters_;
}

std::map<SessionID, bool> OnTaskBlocklist::has_performed_one_level_deep() {
  return has_performed_one_level_deep_;
}

OnTaskBlocklist::RestrictionLevel
OnTaskBlocklist::current_page_restriction_level() {
  return current_page_restriction_level_;
}

void OnTaskBlocklist::CleanupBlocklist() {
  url_blocklist_manager_->SetOverrideBlockListSource(nullptr);
  parent_tab_to_nav_filters_.clear();
  child_tab_to_nav_filters_.clear();
  has_performed_one_level_deep_.clear();
}

// OnTaskBlock::BlocklistSource Implementation
OnTaskBlocklist::OnTaskBlocklistSource::OnTaskBlocklistSource(
    const GURL& url,
    OnTaskBlocklist::RestrictionLevel restriction_type) {
  switch (restriction_type) {
    case OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation:
    case OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation:
    case OnTaskBlocklist::RestrictionLevel::kNoRestrictions:
      allowlist_ = GetAllTrafficFilter();
      return;
    case OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation:
      blocklist_ = GetAllTrafficFilter();
      allowlist_ = GetDomainLevelTrafficFilter(url);
      return;
    case OnTaskBlocklist::RestrictionLevel::kLimitedNavigation:
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
