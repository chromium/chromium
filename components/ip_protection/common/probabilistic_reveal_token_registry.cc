// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/probabilistic_reveal_token_registry.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/strings/string_tokenizer.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace ip_protection {

namespace {
constexpr char kDomainsFieldName[] = "domains";
}  // namespace

ProbabilisticRevealTokenRegistry::ProbabilisticRevealTokenRegistry() {
  if (net::features::kUseCustomProbabilisticRevealTokenRegistry.Get()) {
    std::string custom_registry_csv =
        net::features::kCustomProbabilisticRevealTokenRegistry.Get();
    base::StringTokenizer registry_tokenizer(custom_registry_csv, ",");
    while (registry_tokenizer.GetNext()) {
      custom_domains_.insert(base::ToLowerASCII(registry_tokenizer.token()));
    }
  }
}

ProbabilisticRevealTokenRegistry::~ProbabilisticRevealTokenRegistry() = default;

ProbabilisticRevealTokenRegistry::ProbabilisticRevealTokenRegistry(
    const ProbabilisticRevealTokenRegistry&) = default;

ProbabilisticRevealTokenRegistry& ProbabilisticRevealTokenRegistry::operator=(
    const ProbabilisticRevealTokenRegistry&) = default;

bool ProbabilisticRevealTokenRegistry::IsRegistered(const GURL& request_url) {
  // Check if the eTLD+1 of the request URL is registered.
  std::string request_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          request_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  // Remove the trailing dot if it exists.
  if (request_domain.size() > 0 && request_domain.back() == '.') {
    request_domain = request_domain.substr(0, request_domain.length() - 1);
  }

  if (net::features::kUseCustomProbabilisticRevealTokenRegistry.Get()) {
    return custom_domains_.contains(base::ToLowerASCII(request_domain));
  }

  return domains_.contains(base::ToLowerASCII(request_domain));
}

void ProbabilisticRevealTokenRegistry::UpdateRegistry(
    base::Value::Dict registry) {
  domains_.clear();

  base::Value* domains = registry.Find(kDomainsFieldName);
  if (!domains) {
    return;
  }

  base::Value::List* domain_list = domains->GetIfList();
  if (!domain_list) {
    return;
  }

  for (const base::Value& domain : *domain_list) {
    const std::string* domain_string = domain.GetIfString();
    if (!domain_string) {
      return;
    }
    domains_.insert(base::ToLowerASCII(*domain_string));
  }
}

}  // namespace ip_protection
