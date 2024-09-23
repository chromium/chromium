// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_metrics_util.h"

#include <algorithm>
#include <string_view>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace page_load_metrics {

namespace {

// Default timer delay value.
const int kBaseBufferTimerDelayMillis = 100;

// Additional delay for the browser timer relative to the renderer timer, to
// allow for some variability in task queuing duration and IPC latency.
const int kExtraBufferTimerDelayMillis = 50;

}  // namespace

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

  if (hostname_minus_registry == "google")
    return std::string("");

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

std::optional<base::TimeDelta> OptionalMin(
    const std::optional<base::TimeDelta>& a,
    const std::optional<base::TimeDelta>& b) {
  if (a && !b)
    return a;
  if (b && !a)
    return b;
  if (!a && !b)
    return a;  // doesn't matter which
  return std::optional<base::TimeDelta>(std::min(a.value(), b.value()));
}

int GetBufferTimerDelayMillis(TimerType timer_type) {
  int result = kBaseBufferTimerDelayMillis;

  DCHECK(timer_type == TimerType::kBrowser ||
         timer_type == TimerType::kRenderer);
  if (timer_type == TimerType::kBrowser) {
    result += kExtraBufferTimerDelayMillis;
  }

  return result;
}

}  // namespace page_load_metrics
