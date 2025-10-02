// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/google_url_util.h"

#include <algorithm>
#include <string_view>

#include "base/strings/string_util.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace page_load_metrics {

std::optional<std::string> GetGoogleHostnamePrefix(const GURL& url) {
  const size_t registry_length =
      net::registry_controlled_domains::GetRegistryLength(
          url,

          // Do not include unknown registries (registries that don't have any
          // matches in effective TLD names).
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,

          // Do not include private registries, such as appspot.com. We don't
          // want to match URLs like www.google.appspot.com.
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  const std::string_view hostname = url.host_piece();
  if (registry_length == 0 || registry_length == std::string::npos ||
      registry_length >= hostname.length()) {
    return std::nullopt;
  }

  // Removes the tld and the preceding dot.
  const std::string_view hostname_minus_registry =
      hostname.substr(0, hostname.length() - (registry_length + 1));

  if (hostname_minus_registry == "google") {
    return std::string("");
  }

  if (!base::EndsWith(hostname_minus_registry, ".google",
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return std::nullopt;
  }

  return std::string(hostname_minus_registry.substr(
      0, hostname_minus_registry.length() - strlen(".google")));
}

bool IsGoogleHostname(const GURL& url) {
  return GetGoogleHostnamePrefix(url).has_value();
}

bool IsGoogleSearchHostname(const GURL& url) {
  std::optional<std::string> result =
      page_load_metrics::GetGoogleHostnamePrefix(url);
  return result && result.value() == "www";
}

bool IsProbablyGoogleSearchUrl(const GURL& url) {
  if (!page_load_metrics::IsGoogleSearchHostname(url)) {
    return false;
  }

  const std::string_view path = url.path_piece();
  if (path == "/maps" || path.find("/maps/") != std::string_view::npos) {
    return false;
  }

  return true;
}

// Determine if the given url has query associated with it.
bool HasGoogleSearchQuery(const GURL& url) {
  // NOTE: we do not require 'q=' in the query, as AJAXy search may instead
  // store the query in the URL fragment.
  return QueryContainsComponentPrefix(url.query_piece(), "q=") ||
         QueryContainsComponentPrefix(url.ref_piece(), "q=");
}

bool IsGoogleSearchResultUrl(const GURL& url) {
  if (!IsGoogleSearchHostname(url)) {
    return false;
  }

  if (!HasGoogleSearchQuery(url)) {
    return false;
  }

  const std::string_view path = url.path_piece();
  return path == "/search" || path == "/webhp" || path == "/custom" ||
         path == "/";
}

bool IsGoogleSearchHomepageUrl(const GURL& url) {
  if (!IsGoogleSearchHostname(url)) {
    return false;
  }

  const std::string_view path = url.path_piece();
  if (path == "/webhp" || path == "/") {
    return true;
  }

  return (path == "/custom" || path == "/search") && !HasGoogleSearchQuery(url);
}

bool IsGoogleSearchRedirectorUrl(const GURL& url) {
  if (!IsGoogleSearchHostname(url)) {
    return false;
  }

  // The primary search redirector.  Google search result redirects are
  // differentiated from other general google redirects by 'source=web' in the
  // query string.
  if (url.path_piece() == "/url" && url.has_query() &&
      QueryContainsComponent(url.query_piece(), "source=web")) {
    return true;
  }

  // Intent-based navigations from search are redirected through a second
  // redirector, which receives its redirect URL in the fragment/hash/ref
  // portion of the URL (the portion after '#'). We don't check for the presence
  // of certain params in the ref since this redirector is only used for
  // redirects from search.
  return url.path_piece() == "/searchurl/r.html" && url.has_ref();
}

}  // namespace page_load_metrics
