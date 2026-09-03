// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_CONSTANTS_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_CONSTANTS_H_

#include <string>

#include "base/command_line.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"
#include "url/gurl.h"

namespace one_time_tokens {

// Header name and value for user-facing criticality.
inline constexpr char kOneTimeTokenServiceCriticalityHeaderName[] =
    "x-goog-ext-174067345-bin";
inline constexpr char kOneTimeTokenServiceCriticalityHeaderValue[] = "CgIIAg==";

// Returns the full GURL for a given path on the OneTimeToken service.
inline GURL GetOneTimeTokenServiceUrl(const std::string& path) {
  std::string base_url_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kOneTimeTokenServiceBaseUrl);
  if (base_url_str.empty()) {
    base_url_str = switches::kDefaultOneTimeTokenServiceBaseUrl;
  }
  std::string url_str = base_url_str;
  if (!url_str.empty() && url_str.back() != '/') {
    url_str += "/";
  }
  return GURL(url_str + path);
}

inline constexpr char kTickleArrivalHistogram[] =
    "Autofill.OneTimeTokens.Tickle.Arrival";

// Represents the arrival order and timing of push notifications (tickles)
// relative to OTP form detection on a web page.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(OneTimeTokensTickleArrival)
enum class TickleArrival {
  kAfterFieldDetection = 0,
  kBeforeFieldDetection = 1,
  kWithoutFieldDetection = 2,
  kExpiredOnArrival = 3,
  kMaxValue = kExpiredOnArrival,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:OneTimeTokensTickleArrival)

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_CONSTANTS_H_
