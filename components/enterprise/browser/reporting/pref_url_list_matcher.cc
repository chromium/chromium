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
  path_length_.clear();
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
        url_matcher_.get(), id++, components.scheme, components.host,
        components.match_subdomains, components.port, components.path,
        components.query, components.allow);
    conditions.push_back(condition);
    path_length_.push_back(components.path.size());
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
  size_t maximum_path_length = 0;
  base::MatcherStringPattern::ID maximum_path_id;
  for (const auto id : matched_ids) {
    if (path_length_[id] >= maximum_path_length) {
      maximum_path_id = id;
      maximum_path_length = path_length_[id];
    }
  }
  return pref_service_->GetList(pref_name_)[maximum_path_id].GetString();
}

}  // namespace enterprise_reporting
