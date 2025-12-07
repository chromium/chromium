// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/omnibox_autofocus_http_headers.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/google/core/common/google_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace variations {

// The `X-Omnibox-Autofocus` header, used on Android to report the state of the
// `kOmniboxAutofocusOnIncognitoNtp` feature.
// The header can have one of 9 values:
// - "-1": The feature is disabled.
// - "0" through "7": When the feature is enabled, this is a bitmask of the
//   following feature parameters:
//   - bit 0 (rightmost): `with_hardware_keyboard`
//   - bit 1: `with_prediction`
//   - bit 2 (leftmost): `not_first_tab`
const char kOmniboxAutofocusHeaderName[] = "X-Omnibox-Autofocus";

// Whether to enable reporting the header. Included as a quick escape hatch in
// case of crashes.
BASE_FEATURE(kReportOmniboxAutofocusHeader, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature that controls the Omnibox Autofocus experiment.
BASE_FEATURE(kOmniboxAutofocusOnIncognitoNtp,
             base::FEATURE_DISABLED_BY_DEFAULT);
// When true, omnibox autofocus is enabled if it's not the first tab seen in
// the current incognito browser session.
BASE_FEATURE_PARAM(bool,
                   kNotFirstTab,
                   &kOmniboxAutofocusOnIncognitoNtp,
                   "not_first_tab",
                   false);
// When true, omnibox autofocus is enabled if a hardware keyboard is detected.
BASE_FEATURE_PARAM(bool,
                   kWithHardwareKeyboard,
                   &kOmniboxAutofocusOnIncognitoNtp,
                   "with_hardware_keyboard",
                   false);
// When true, omnibox autofocus is enabled if we predict the software keyboard
// will not hide more than the approved portion (around 25%) of the incognito
// NTP text content.
BASE_FEATURE_PARAM(bool,
                   kWithPrediction,
                   &kOmniboxAutofocusOnIncognitoNtp,
                   "with_prediction",
                   false);

std::string GetHeaderValue() {
  std::string header_value;
  if (base::FeatureList::IsEnabled(kOmniboxAutofocusOnIncognitoNtp)) {
    const bool not_first_tab = kNotFirstTab.Get();
    const bool with_prediction = kWithPrediction.Get();
    const bool with_hardware_keyboard = kWithHardwareKeyboard.Get();

    int value =
        (not_first_tab << 2) | (with_prediction << 1) | with_hardware_keyboard;
    header_value = base::NumberToString(value);
  } else {
    header_value = "-1";
  }
  return header_value;
}

bool ShouldAppendHeader(const GURL& url) {
  return url.SchemeIs(url::kHttpsScheme) &&
         google_util::IsGoogleDomainUrl(
             url, google_util::ALLOW_SUBDOMAIN,
             google_util::DISALLOW_NON_STANDARD_PORTS);
}

void AppendOmniboxAutofocusHeaderIfNeeded(const GURL& url,
                                          network::ResourceRequest* request) {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kReportOmniboxAutofocusHeader)) {
    return;
  }

  if (!ShouldAppendHeader(url)) {
    return;
  }

  std::string header = GetHeaderValue();
  // Set the omnibox header to cors_exempt_headers rather than headers
  // to be exempted from CORS checks.
  request->cors_exempt_headers.SetHeaderIfMissing(kOmniboxAutofocusHeaderName,
                                                  header);
#endif  // BUILDFLAG(IS_ANDROID)
}

void UpdateCorsExemptHeaderForOmniboxAutofocus(
    network::mojom::NetworkContextParams* params) {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(kReportOmniboxAutofocusHeader)) {
    params->cors_exempt_header_list.push_back(kOmniboxAutofocusHeaderName);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace variations
