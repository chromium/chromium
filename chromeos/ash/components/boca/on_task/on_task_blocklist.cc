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
  return url_blocklist_manager_->GetURLBlocklistState(url);
}

void OnTaskBlocklist::SetURLRestrictionLevel(
    const GURL& url,
    OnTaskBlocklist::RestrictionLevel restriction_level) {
  std::string url_content = url.GetContent();
  RemovePrefix(url_content, GetCommonUrlPrefix());
  if (base::Contains(parent_tab_url_to_nav_filters_, url_content)) {
    parent_tab_url_to_nav_filters_[url_content] = restriction_level;
  } else {
    child_tab_url_to_nav_filters_[url_content] = restriction_level;
  }
}

void OnTaskBlocklist::SetParentURLRestrictionLevel(
    const GURL& url,
    OnTaskBlocklist::RestrictionLevel restriction_level) {
  std::string url_content = url.GetContent();
  RemovePrefix(url_content, GetCommonUrlPrefix());
  parent_tab_url_to_nav_filters_[url_content] = restriction_level;
  if (restriction_level ==
          OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation ||
      restriction_level ==
          OnTaskBlocklist::RestrictionLevel::kDomainAndOneLevelDeepNavigation) {
    std::string domain_url = url.GetWithEmptyPath().GetContent();
    RemovePrefix(domain_url, GetCommonUrlPrefix());
    parent_tab_url_to_nav_filters_[domain_url] = restriction_level;
  }
}

void OnTaskBlocklist::RefreshForUrlBlocklist(const GURL& url) {
  if (previous_url_.is_valid() && previous_url_ == url) {
    return;
  }
  std::string url_content = url.GetContent();
  std::string url_no_file_name = url.GetWithoutFilename().GetContent();
  std::string url_empty_path_name = url.GetWithEmptyPath().GetContent();
  RemovePrefix(url_content, GetCommonUrlPrefix());
  RemovePrefix(url_no_file_name, GetCommonUrlPrefix());
  RemovePrefix(url_empty_path_name, GetCommonUrlPrefix());

  std::unique_ptr<OnTaskBlocklistSource> blocklist_source;
  // Updates the blocklist given the active tab's url. This function does a
  // series of checks to determine what restriction levels apply. It starts at
  // closest match starting from the child maps and continues outwards to least
  // restrictive url matching in case urls have been redirected or have its url
  // rewritten (ex. google drive home page to user authenticated google drive
  // home page.). Note: The navigation throttler is responsible for updating the
  // set of urls and their restriction levels. In case that there are duplicate
  // urls that are being navigated to with different restriction levels, it
  // looks to respect the initial restriction levels if the url currently being
  // navigated falls within those initial sets of urls.
  if (base::Contains(child_tab_url_to_nav_filters_, url_content) ||
      base::Contains(child_tab_url_to_nav_filters_, url_no_file_name) ||
      base::Contains(child_tab_url_to_nav_filters_, url_empty_path_name)) {
    OnTaskBlocklist::RestrictionLevel restriction_level;
    if (base::Contains(child_tab_url_to_nav_filters_, url_content)) {
      restriction_level = child_tab_url_to_nav_filters_[url_content];
    } else if (base::Contains(child_tab_url_to_nav_filters_,
                              url_no_file_name)) {
      restriction_level = child_tab_url_to_nav_filters_[url_no_file_name];
    } else {
      restriction_level = child_tab_url_to_nav_filters_[url_empty_path_name];
    }
    blocklist_source =
        std::make_unique<OnTaskBlocklistSource>(url, restriction_level);
    current_page_restriction_level_ = restriction_level;

  } else if (base::Contains(parent_tab_url_to_nav_filters_, url_content) ||
             base::Contains(parent_tab_url_to_nav_filters_, url_no_file_name) ||
             base::Contains(parent_tab_url_to_nav_filters_,
                            url_empty_path_name)) {
    OnTaskBlocklist::RestrictionLevel restriction_level;

    if (base::Contains(parent_tab_url_to_nav_filters_, url_content)) {
      restriction_level = parent_tab_url_to_nav_filters_[url_content];
    } else if (base::Contains(parent_tab_url_to_nav_filters_,
                              url_no_file_name)) {
      restriction_level = parent_tab_url_to_nav_filters_[url_no_file_name];
    } else {
      restriction_level = parent_tab_url_to_nav_filters_[url_empty_path_name];
    }
    blocklist_source =
        std::make_unique<OnTaskBlocklistSource>(url, restriction_level);
    current_page_restriction_level_ = restriction_level;

  } else {
    // Should only happen if a url redirect changes the active tab's url.
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

void OnTaskBlocklist::RemoveChildFilter(const GURL& url) {
  if (base::Contains(child_tab_url_to_nav_filters_, url.GetContent())) {
    child_tab_url_to_nav_filters_.erase((url.GetContent()));
  }
}

const policy::URLBlocklistManager* OnTaskBlocklist::url_blocklist_manager() {
  return url_blocklist_manager_.get();
}

std::map<std::string, OnTaskBlocklist::RestrictionLevel>
OnTaskBlocklist::parent_tab_url_to_nav_filters() {
  return parent_tab_url_to_nav_filters_;
}

std::map<std::string, OnTaskBlocklist::RestrictionLevel>
OnTaskBlocklist::child_tab_url_to_nav_filters() {
  return child_tab_url_to_nav_filters_;
}

OnTaskBlocklist::RestrictionLevel
OnTaskBlocklist::current_page_restriction_level() {
  return current_page_restriction_level_;
}

void OnTaskBlocklist::CleanupBlocklist() {
  url_blocklist_manager_->SetOverrideBlockListSource(nullptr);
  parent_tab_url_to_nav_filters_.clear();
  child_tab_url_to_nav_filters_.clear();
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
