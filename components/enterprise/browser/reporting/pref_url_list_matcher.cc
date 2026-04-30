// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/pref_url_list_matcher.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace enterprise_reporting {

PrefURLListMatcher::PrefURLListMatcher(PrefService* pref_service,
                                       const char* pref_name)
    : pref_service_(pref_service), pref_name_(pref_name) {
  pref_change_.Init(pref_service_);
  pref_change_.Add(pref_name_,
                   base::BindRepeating(&PrefURLListMatcher::OnPrefUpdated,
                                       base::Unretained(this)));

  OnPrefUpdated();
}

PrefURLListMatcher::~PrefURLListMatcher() = default;

void PrefURLListMatcher::OnPrefUpdated() {
  base::MatcherStringPattern::ID id = 0;
  url_matcher::URLMatcherConditionSet::Vector conditions;
  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  filters_.clear();
  for (const auto& url : pref_service_->GetList(pref_name_)) {
    url_matcher::util::FilterComponents components;
    url_matcher::util::FilterToComponents(
        url.GetString(), &components.scheme, &components.host,
        &components.match_subdomains, &components.port, &components.path,
        &components.query);

    // Scheme, port and query in the pattern will be ignored while subdomains
    // must be fully specified.
    components.scheme = "";
    components.port = 0;
    components.query = "";

    auto condition = url_matcher::util::CreateConditionSet(
        url_matcher_.get(), id, components.scheme, components.host,
        components.match_subdomains, components.port, components.path,
        components.query, components.allow);
    conditions.push_back(condition);
    filters_.emplace(id++, std::move(components));
  }
  url_matcher_->AddConditionSets(conditions);
}

std::optional<std::string> PrefURLListMatcher::GetMatchedURL(
    const GURL& url) const {
  std::set<base::MatcherStringPattern::ID> matched_ids =
      url_matcher_->MatchURL(url);

  if (matched_ids.empty()) {
    return std::nullopt;
  }

  base::MatcherStringPattern::ID best_match_id = *matched_ids.begin();
  for (const auto id : matched_ids) {
    if (IsHigherPriority(id, best_match_id)) {
      best_match_id = id;
    }
  }

  return pref_service_->GetList(pref_name_)[best_match_id].GetString();
}

// Returns true if `lhs` filter is higher priority than `rhs` filter.
// See the class comment for the priority rules.
bool PrefURLListMatcher::IsHigherPriority(
    base::MatcherStringPattern::ID lhs,
    base::MatcherStringPattern::ID rhs) const {
  const auto& lhs_filter = filters_.at(lhs);
  const auto& rhs_filter = filters_.at(rhs);

  // 1. Exact host matches (.example.com) beat subdomain matches (example.com)
  if (lhs_filter.match_subdomains != rhs_filter.match_subdomains) {
    // If lhs_filter.match_subdomains is false, it means the filter is an exact
    // host match, which is higher priority than a subdomain match.
    return !lhs_filter.match_subdomains;
  }

  // 2. Longer host wins
  if (lhs_filter.host.length() != rhs_filter.host.length()) {
    return lhs_filter.host.length() > rhs_filter.host.length();
  }

  // 3. Longer path wins
  if (lhs_filter.path.length() != rhs_filter.path.length()) {
    return lhs_filter.path.length() > rhs_filter.path.length();
  }

  // 4. Later entry wins
  return lhs > rhs;
}

}  // namespace enterprise_reporting
