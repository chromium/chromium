// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/safe_search_util.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/google/core/common/google_util.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace {

// Returns whether a URL parameter, |first_parameter| (e.g. foo=bar), has the
// same key as the the |second_parameter| (e.g. foo=baz). Both parameters
// must be in key=value form.
bool HasSameParameterKey(std::string_view first_parameter,
                         std::string_view second_parameter) {
  DCHECK(second_parameter.find("=") != std::string::npos);
  // Prefix for "foo=bar" is "foo=".
  std::string_view parameter_prefix =
      second_parameter.substr(0, second_parameter.find("=") + 1);
  return base::StartsWith(first_parameter, parameter_prefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

// Examines the query string containing parameters and adds the necessary ones
// so that SafeSearch is active. |query| is the string to examine and the
// return value is the |query| string modified such that SafeSearch is active.
std::string AddSafeSearchParameters(const std::string& query) {
  std::vector<std::string_view> new_parameters;
  std::string safe_parameter = safe_search_api::kSafeSearchSafeParameter;
  std::string ssui_parameter = safe_search_api::kSafeSearchSsuiParameter;

  for (std::string_view param : base::SplitStringPiece(
           query, "&", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (!HasSameParameterKey(param, safe_parameter) &&
        !HasSameParameterKey(param, ssui_parameter)) {
      new_parameters.push_back(param);
    }
  }

  new_parameters.push_back(safe_parameter);
  new_parameters.push_back(ssui_parameter);
  return base::JoinString(new_parameters, "&");
}

bool IsSafeSearchSettingUrl(const GURL& url) {
  if (!google_util::IsGoogleDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return false;
  }

  return url.path_piece() == "/safesearch";
}

}  // namespace

namespace safe_search_api {

const char kSafeSearchSafeParameter[] = "safe=active";
const char kSafeSearchSsuiParameter[] = "ssui=on";
const char kYouTubeRestrictHeaderName[] = "YouTube-Restrict";
const char kYouTubeRestrictHeaderValueModerate[] = "Moderate";
const char kYouTubeRestrictHeaderValueStrict[] = "Strict";
const char kGoogleAppsAllowedDomains[] = "X-GoogApps-Allowed-Domains";

// If |request| is a request to Google Web Search the function
// enforces that the SafeSearch query parameters are set to active.
// Sets the query part of |new_url| with the new value of the parameters.
void ForceGoogleSafeSearch(const GURL& url, GURL* new_url) {
  if (!google_util::IsGoogleSearchUrl(url) &&
      !google_util::IsGoogleHomePageUrl(url) && !IsSafeSearchSettingUrl(url)) {
    return;
  }

  std::string query = url.query();
  std::string new_query = AddSafeSearchParameters(query);
  if (query == new_query) {
    return;
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query);
  *new_url = url.ReplaceComponents(replacements);
}

void ForceYouTubeRestrict(const GURL& url,
                          net::HttpRequestHeaders* headers,
                          YouTubeRestrictMode mode) {
  if (!google_util::IsYoutubeDomainUrl(
          url, google_util::ALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS)) {
    return;
  }

  switch (mode) {
    case YOUTUBE_RESTRICT_OFF:
    case YOUTUBE_RESTRICT_COUNT:
      NOTREACHED_IN_MIGRATION();
      break;

    case YOUTUBE_RESTRICT_MODERATE:
      headers->SetHeader(kYouTubeRestrictHeaderName,
                         kYouTubeRestrictHeaderValueModerate);
      break;

    case YOUTUBE_RESTRICT_STRICT:
      headers->SetHeader(kYouTubeRestrictHeaderName,
                         kYouTubeRestrictHeaderValueStrict);
      break;
  }
}

}  // namespace safe_search_api
