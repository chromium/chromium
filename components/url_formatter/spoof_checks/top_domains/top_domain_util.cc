// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"

#include <optional>
#include <string_view>

#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace url_formatter::top_domains {

namespace {

// Minimum length of the e2LD (the registered domain name without the registry)
// to be considered for an edit distance comparison.
// Thus: 'google.com' has of length 6 ("google") and is long enough, while
//       'abc.co.uk' has a length of 3 ("abc"), and will not be considered.
const size_t kMinLengthForEditDistance = 5u;

}  // namespace

bool IsEditDistanceCandidate(const std::string& hostname) {
  return !hostname.empty() &&
         HostnameWithoutRegistry(hostname).size() >= kMinLengthForEditDistance;
}

std::string HostnameWithoutRegistry(const std::string& hostname) {
  DCHECK(!hostname.empty());
  const size_t registry_size =
      net::registry_controlled_domains::PermissiveGetHostRegistry(
          hostname.c_str(),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)
          .transform(&std::string_view::size)
          .value_or(std::string_view::npos);
  std::string out = hostname.substr(0, hostname.size() - registry_size);
  base::TrimString(out, ".", &out);
  return out;
}

}  // namespace url_formatter::top_domains
