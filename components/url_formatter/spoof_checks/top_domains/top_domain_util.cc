// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace url_formatter {

namespace top_domains {

namespace {

// Minimum length of the e2LD (the registered domain name without the registry)
// to be considered for an edit distance comparison, including a trailing dot.
// Thus: 'google.com' has of length 7 ("google.") and is long enough, while
//       'abc.co.uk' has a length of 4 ("abc."), and will not be considered.
const size_t kMinLengthForEditDistance = 6u;

}  // namespace

bool IsEditDistanceCandidate(const std::string& hostname) {
  return !hostname.empty() &&
         HostnameWithoutRegistry(hostname).size() >= kMinLengthForEditDistance;
}

std::string HostnameWithoutRegistry(const std::string& hostname) {
  DCHECK(!hostname.empty());
  const size_t registry_size =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          hostname.c_str(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  return hostname.substr(0, hostname.size() - registry_size);
}

}  // namespace top_domains

}  // namespace url_formatter
