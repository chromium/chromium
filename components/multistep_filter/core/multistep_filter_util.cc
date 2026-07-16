// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_util.h"

#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace multistep_filter {

std::string GetEtldPlusOneForHost(std::string_view host) {
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {
    return std::string(host);
  }
  return domain;
}

std::string GetEtldPlusOne(const GURL& url) {
  return GetEtldPlusOneForHost(url.host());
}

bool IsSameDomainOrHost(const GURL& url, const GURL& other) {
  return GetEtldPlusOne(url) == GetEtldPlusOne(other);
}

bool IsUrlSubsumedBy(const GURL& candidate_url, const GURL& reference_url) {
  if (!candidate_url.is_valid() || !reference_url.is_valid()) {
    return false;
  }

  if (candidate_url == reference_url) {
    return true;
  }

  // If base URLs (everything except query and ref) are different, it's not
  // redundant.
  GURL::Replacements replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  if (candidate_url.ReplaceComponents(replacements) !=
      reference_url.ReplaceComponents(replacements)) {
    return false;
  }

  // Extract query parameters from both URLs.
  std::multiset<std::pair<std::string_view, std::string>> reference_params;
  for (net::QueryIterator it(reference_url); !it.IsAtEnd(); it.Advance()) {
    reference_params.emplace(it.GetKey(), it.GetUnescapedValue());
  }

  // If the candidate parameters are a subset of (or identical to) the
  // reference URL parameters, the candidate is redundant.
  for (net::QueryIterator it(candidate_url); !it.IsAtEnd(); it.Advance()) {
    auto match = reference_params.find(
        std::make_pair(it.GetKey(), it.GetUnescapedValue()));
    if (match == reference_params.end()) {
      return false;
    }
    // Erase the matched parameter to handle duplicate keys correctly.
    reference_params.erase(match);
  }

  return true;
}

}  // namespace multistep_filter
