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

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_SERVICE_CONSTANTS_H_
