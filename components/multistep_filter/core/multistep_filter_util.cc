// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/multistep_filter_util.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/multistep_filter/core/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {
constexpr std::string_view kWildcardDomain = "*";
}  // namespace

std::string GetEtldPlusOne(const GURL& url) {
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {
    return std::string(url.host());
  }
  return domain;
}

bool IsUrlAllowed(const GURL& url) {
  if (!base::FeatureList::IsEnabled(kMultistepFilter)) {
    return false;
  }
  // TODO (crbug.com/493208014): Use a more robust solution for checking if the
  // URL is cataloged by the annotation index.
  std::string allowed_domains = kMultistepFilterAllowedDomains.Get();
  std::vector<std::string_view> domains = base::SplitStringPiece(
      allowed_domains, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (std::ranges::contains(domains, kWildcardDomain)) {
    return true;
  }

  std::string domain_and_registry = GetEtldPlusOne(url);

  return std::ranges::any_of(domains, [&](std::string_view domain) {
    return base::EqualsCaseInsensitiveASCII(domain_and_registry, domain);
  });
}

bool IsSameDomainOrHost(const GURL& url, const GURL& other) {
  return GetEtldPlusOne(url) == GetEtldPlusOne(other);
}

}  // namespace multistep_filter
