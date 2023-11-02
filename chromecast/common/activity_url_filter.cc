// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/activity_url_filter.h"

namespace chromecast {

ActivityUrlFilter::ActivityUrlFilter(
    const std::vector<std::string>& url_filters)
    : url_matcher_(std::make_unique<url_matcher::URLMatcher>()) {
  base::MatcherStringPattern::ID id = 0;
  url_matcher::URLMatcherConditionSet::Vector condition_sets;
  for (const auto& url : url_filters) {
    url_matcher::URLMatcherConditionSet::Conditions conditions;
    conditions.insert(
        url_matcher_->condition_factory()->CreateURLMatchesCondition(url));
    scoped_refptr<url_matcher::URLMatcherConditionSet> condition_set =
        new url_matcher::URLMatcherConditionSet(id++, conditions);
    condition_sets.push_back(std::move(condition_set));
  }
  url_matcher_->AddConditionSets(condition_sets);
}

ActivityUrlFilter::~ActivityUrlFilter() = default;

bool ActivityUrlFilter::UrlMatchesWhitelist(const GURL& url) {
  return !url_matcher_->MatchURL(url).empty();
}

}  // namespace chromecast
