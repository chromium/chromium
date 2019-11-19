// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_metrics_util.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace page_load_metrics {

base::Optional<std::string> GetGoogleHostnamePrefix(const GURL& url) {
  const size_t registry_length =
      net::registry_controlled_domains::GetRegistryLength(
          url,

          // Do not include unknown registries (registries that don't have any
          // matches in effective TLD names).
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,

          // Do not include private registries, such as appspot.com. We don't
          // want to match URLs like www.google.appspot.com.
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  const base::StringPiece hostname = url.host_piece();
  if (registry_length == 0 || registry_length == std::string::npos ||
      registry_length >= hostname.length()) {
    return base::Optional<std::string>();
  }

  // Removes the tld and the preceding dot.
  const base::StringPiece hostname_minus_registry =
      hostname.substr(0, hostname.length() - (registry_length + 1));

  if (hostname_minus_registry == "google")
    return std::string("");

  if (!base::EndsWith(hostname_minus_registry, ".google",
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return base::Optional<std::string>();
  }

  return std::string(hostname_minus_registry.substr(
      0, hostname_minus_registry.length() - strlen(".google")));
}

bool IsGoogleHostname(const GURL& url) {
  return GetGoogleHostnamePrefix(url).has_value();
}

base::Optional<base::TimeDelta> OptionalMin(
    const base::Optional<base::TimeDelta>& a,
    const base::Optional<base::TimeDelta>& b) {
  if (a && !b)
    return a;
  if (b && !a)
    return b;
  if (!a && !b)
    return a;  // doesn't matter which
  return base::Optional<base::TimeDelta>(std::min(a.value(), b.value()));
}

}  // namespace page_load_metrics
