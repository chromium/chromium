// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/omnibox_http_headers.h"

#include "base/metrics/field_trial.h"
#include "components/google/core/common/google_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace variations {

// The header used to report the state of the omnibox on-device search
// suggestions provider. This header is only set when the provider is in a
// non-default state, and only reports one of two possible values: "Enabled_V2"
// or "Control_V2".
const char kOmniboxOnDeviceSuggestionsHeader[] =
    "X-Omnibox-On-Device-Suggestions";

// Whether to enable reporting the header. Included as a quick escape hatch in
// case of crashes.
const base::Feature kReportOmniboxOnDeviceSuggestionsHeader{
    "ReportOmniboxOnDeviceSuggestionsHeader", base::FEATURE_ENABLED_BY_DEFAULT};

std::string GetHeaderValue() {
  const std::string group =
      base::FieldTrialList::FindFullName("OmniboxBundledExperimentV1");

  // Search for a substring rather than comparing to an exact value so that
  // group names can have prefixes (e.g., "Desktop", "Android") and suffixes
  // (e.g., config versions).
  if (group.find("ReportHttpHeader_Enabled_V2") != std::string::npos) {
    return "Enabled_V2";
  }
  if (group.find("ReportHttpHeader_Control_V2") != std::string::npos) {
    return "Control_V2";
  }

  return std::string();
}

// Returns whether the header should be sent for this URL. The header is only
// sent to Google domains served over HTTPS.
bool ShouldAppendHeader(const GURL& url) {
  return url.SchemeIs(url::kHttpsScheme) &&
         google_util::IsGoogleDomainUrl(
             url, google_util::ALLOW_SUBDOMAIN,
             google_util::DISALLOW_NON_STANDARD_PORTS);
}

void AppendOmniboxOnDeviceSuggestionsHeaderIfNeeded(
    const GURL& url,
    network::ResourceRequest* request) {
  if (!base::FeatureList::IsEnabled(kReportOmniboxOnDeviceSuggestionsHeader))
    return;

  if (!ShouldAppendHeader(url))
    return;

  std::string header = GetHeaderValue();
  if (header.empty())
    return;

  // Set the omnibox header to cors_exempt_headers rather than headers
  // to be exempted from CORS checks.
  request->cors_exempt_headers.SetHeaderIfMissing(
      kOmniboxOnDeviceSuggestionsHeader, header);
}

}  // namespace variations
