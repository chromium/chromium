// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_OMNIBOX_AUTOFOCUS_HTTP_HEADERS_H_
#define COMPONENTS_VARIATIONS_NET_OMNIBOX_AUTOFOCUS_HTTP_HEADERS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

class GURL;

namespace network {
struct ResourceRequest;
namespace mojom {
class NetworkContextParams;
}  // namespace mojom
}  // namespace network

namespace variations {

COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
extern const char kOmniboxAutofocusHeaderName[];

COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
BASE_DECLARE_FEATURE(kReportOmniboxAutofocusHeader);

// Feature that controls the Omnibox Autofocus experiment.
COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
BASE_DECLARE_FEATURE(kOmniboxAutofocusOnIncognitoNtp);
COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
BASE_DECLARE_FEATURE_PARAM(bool, kNotFirstTab);
COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
BASE_DECLARE_FEATURE_PARAM(bool, kWithHardwareKeyboard);
COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
BASE_DECLARE_FEATURE_PARAM(bool, kWithPrediction);

// Returns the value to set for the header.
COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
std::string GetHeaderValue();

// Returns whether the header should be sent for this URL. The header is only
// sent to Google domains served over HTTPS.
COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
bool ShouldAppendHeader(const GURL& url);

// Adds an HTTP header reporting the omnibox autofocus experiment state.
// Only reported if the |url| is a Google domain and
// `kReportOmniboxAutofocusHeader` feature flag is enabled.
// This method is safe to call both from browser and from child processes.
COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
void AppendOmniboxAutofocusHeaderIfNeeded(const GURL& url,
                                          network::ResourceRequest* request);

// This function allowlists the header at the network context level so that it
// can be added to requests without triggering CORS checks. The header may not
// be attached to every request.
COMPONENT_EXPORT(OMNIBOX_AUTOFOCUS_HTTP_HEADERS)
void UpdateCorsExemptHeaderForOmniboxAutofocus(
    network::mojom::NetworkContextParams* params);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_OMNIBOX_AUTOFOCUS_HTTP_HEADERS_H_
