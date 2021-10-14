// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/url_utils.h"

#include <algorithm>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace {

bool IsInSubDomain(const GURL& url, const std::string& domain) {
  return base::EndsWith(base::StringPiece(url.host()),
                        base::StringPiece("." + domain),
                        base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

namespace autofill_assistant {
namespace url_utils {

bool IsInDomainOrSubDomain(const GURL& url, const GURL& domain) {
  if (url.host() == domain.host()) {
    return true;
  }

  return IsInSubDomain(url, domain.host());
}

bool IsInDomainOrSubDomain(const GURL& url,
                           const std::vector<std::string>& allowed_domains) {
  return std::find_if(allowed_domains.begin(), allowed_domains.end(),
                      [url](const std::string& allowed_domain) {
                        return url.host() == allowed_domain ||
                               IsInSubDomain(url, allowed_domain);
                      }) != allowed_domains.end();
}

bool IsSamePublicSuffixDomain(const GURL& url1, const GURL& url2) {
  if (!url1.is_valid() || !url2.is_valid()) {
    return false;
  }

  if (url1.DeprecatedGetOriginAsURL() == url2.DeprecatedGetOriginAsURL()) {
    return true;
  }

  auto domain1 = GetOrganizationIdentifyingDomain(url1);
  auto domain2 = GetOrganizationIdentifyingDomain(url2);
  if (domain1.empty() || domain2.empty()) {
    return false;
  }

  return domain1 == domain2;
}

std::string GetOrganizationIdentifyingDomain(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsAllowedSchemaTransition(const GURL& from, const GURL& to) {
  return from.scheme() == to.scheme() || to.scheme() == url::kHttpsScheme;
}

}  // namespace url_utils
}  // namespace autofill_assistant
