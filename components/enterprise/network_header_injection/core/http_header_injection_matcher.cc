// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/http_header_injection_matcher.h"

#include <algorithm>
#include <map>
#include <set>

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace enterprise_custom_headers {

namespace {

// Determines precedence between two filters based on specificity:
// 1. Non-wildcard matches take precedence over wildcard matches.
// 2. Exact host matches take precedence over subdomain matches.
// 3. Longer host patterns take precedence over shorter ones.
// 4. Longer path patterns take precedence over shorter ones.
// 5. Tie-breaker: later registered rules (higher ID) win on equal specificity.
// Returns true if `lhs` takes precedence over `rhs`.
bool FilterTakesPrecedenceStrict(const url_matcher::util::FilterComponents& lhs,
                                 base::MatcherStringPattern::ID lhs_id,
                                 const url_matcher::util::FilterComponents& rhs,
                                 base::MatcherStringPattern::ID rhs_id) {
  if (lhs.IsWildcard() && !rhs.IsWildcard()) {
    return false;
  }
  if (!lhs.IsWildcard() && rhs.IsWildcard()) {
    return true;
  }

  if (lhs.match_subdomains && !rhs.match_subdomains) {
    return false;
  }
  if (!lhs.match_subdomains && rhs.match_subdomains) {
    return true;
  }

  const size_t host_length = lhs.host.length();
  const size_t other_host_length = rhs.host.length();
  if (host_length != other_host_length) {
    return host_length > other_host_length;
  }

  const size_t path_length = lhs.path.length();
  const size_t other_path_length = rhs.path.length();
  if (path_length != other_path_length) {
    return path_length > other_path_length;
  }

  if (lhs.number_of_url_matching_conditions !=
      rhs.number_of_url_matching_conditions) {
    return lhs.number_of_url_matching_conditions >
           rhs.number_of_url_matching_conditions;
  }

  // Tie-breaker to ensure strict weak ordering (no equal cases):
  return lhs_id > rhs_id;
}

struct CaseInsensitiveHash {
  size_t operator()(std::string_view str) const {
    return base::FastHash(base::ToLowerASCII(str));
  }
};

struct CaseInsensitiveEqual {
  bool operator()(std::string_view lhs, std::string_view rhs) const {
    return base::EqualsCaseInsensitiveASCII(lhs, rhs);
  }
};

}  // namespace

class HttpHeaderInjectionMatcherImpl : public HttpHeaderInjectionMatcher {
 public:
  ~HttpHeaderInjectionMatcherImpl() override;

  // HttpHeaderInjectionMatcher:
  void UpdateRules(const std::vector<HttpHeaderInjectionRule>& rules) override;
  net::HttpRequestHeaders GetHeadersForUrl(const GURL& url) const override;
  bool IsEmpty() const override;

 private:
  struct Rule {
    std::vector<std::pair<std::string, std::string>> headers;
    url_matcher::util::FilterComponents filter_components;
    int precedence_rank = 0;
  };

  // Pre-calculates and assigns precedence ranks for all registered rules.
  void CalculatePrecedenceRanks();

  std::unique_ptr<url_matcher::URLMatcher> url_matcher_ =
      std::make_unique<url_matcher::URLMatcher>();
  absl::flat_hash_map<base::MatcherStringPattern::ID, Rule> rules_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// static
std::unique_ptr<HttpHeaderInjectionMatcher>
HttpHeaderInjectionMatcher::Create() {
  return std::make_unique<HttpHeaderInjectionMatcherImpl>();
}

// --- Implementation of HttpHeaderInjectionMatcherImpl ---

HttpHeaderInjectionMatcherImpl::~HttpHeaderInjectionMatcherImpl() = default;

void HttpHeaderInjectionMatcherImpl::UpdateRules(
    const std::vector<HttpHeaderInjectionRule>& rules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rules_.clear();
  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();

  base::MatcherStringPattern::ID current_id(0);

  for (const auto& rule_data : rules) {
    if (rule_data.url_patterns.empty() || rule_data.headers.empty()) {
      continue;
    }

    std::map<base::MatcherStringPattern::ID,
             url_matcher::util::FilterComponents>
        filters;
    url_matcher::util::AddFiltersWithLimit(
        url_matcher_.get(), /*allow=*/true, &current_id, rule_data.url_patterns,
        &filters, rule_data.url_patterns.size());

    for (auto& [id, filter_components] : filters) {
      Rule rule;
      rule.headers = rule_data.headers;
      rule.filter_components = std::move(filter_components);
      CHECK(!rules_.contains(id));
      rules_[id] = std::move(rule);
    }
  }

  CalculatePrecedenceRanks();
}

net::HttpRequestHeaders HttpHeaderInjectionMatcherImpl::GetHeadersForUrl(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (url.is_empty()) {
    return {};
  }

  std::set<base::MatcherStringPattern::ID> matches =
      url_matcher_->MatchURL(url);

  if (matches.empty()) {
    return {};
  }

  struct HeaderValueAndPrecedence {
    std::string_view value;
    int precedence_rank = -1;
  };

  // Map header name to the value and the rule that takes precedence.
  absl::flat_hash_map<std::string_view, HeaderValueAndPrecedence,
                      CaseInsensitiveHash, CaseInsensitiveEqual>
      header_map;
  for (auto id : matches) {
    auto it = rules_.find(id);
    CHECK(it != rules_.end());
    const Rule& rule = it->second;

    for (const auto& [name, val] : rule.headers) {
      auto [map_it, inserted] = header_map.insert(
          {name, {.value = val, .precedence_rank = rule.precedence_rank}});
      // If the header is already in the map, check which rule takes
      // precedence.
      if (!inserted) {
        HeaderValueAndPrecedence& stored = map_it->second;
        if (rule.precedence_rank > stored.precedence_rank) {
          stored.value = val;
          stored.precedence_rank = rule.precedence_rank;
        }
      }
    }
  }

  net::HttpRequestHeaders headers_to_inject;
  for (const auto& [header_name, info] : header_map) {
    headers_to_inject.SetHeader(header_name, info.value);
  }

  return headers_to_inject;
}

bool HttpHeaderInjectionMatcherImpl::IsEmpty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return rules_.empty();
}

void HttpHeaderInjectionMatcherImpl::CalculatePrecedenceRanks() {
  std::vector<base::MatcherStringPattern::ID> sorted_ids;
  sorted_ids.reserve(rules_.size());
  for (const auto& [id, _] : rules_) {
    sorted_ids.push_back(id);
  }

  std::sort(sorted_ids.begin(), sorted_ids.end(), [this](auto id1, auto id2) {
    // We sort in ascending order of precedence (lowest precedence to
    // highest). So, id1 should come before id2 if id1 has lower
    // precedence than id2.
    return FilterTakesPrecedenceStrict(rules_[id2].filter_components, id2,
                                       rules_[id1].filter_components, id1);
  });

  for (size_t i = 0; i < sorted_ids.size(); ++i) {
    rules_[sorted_ids[i]].precedence_rank = i;
  }
}

}  // namespace enterprise_custom_headers
