// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_H_
#define COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace site_token_provider {

// Normalizes domain by converting to lowercase and stripping the "www." prefix
// so that "www.domain.com" and "domain.com" match the same token, while
// preserving subdomain isolation for other subdomains (e.g. "news.domain.com").
std::string NormalizeDomain(std::string_view domain);

// Parses and normalizes a comma-separated allowlist of domains into a set.
base::flat_set<std::string> ParseAllowlistedDomains(std::string_view allowlist);

// Interface for the core logic of managing site-specific tokens.
class SiteTokenProvider {
 public:
  using TokenUpdateCallback =
      base::RepeatingCallback<void(std::map<std::string, std::string>)>;

  static std::unique_ptr<SiteTokenProvider> Create(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~SiteTokenProvider();

  // Registers the callback to receive site token updates.
  virtual void SetTokenUpdateCallback(TokenUpdateCallback callback) = 0;

  // Triggers local state synchronization updates.
  virtual void UpdateState() = 0;
};

}  // namespace site_token_provider

#endif  // COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_H_
