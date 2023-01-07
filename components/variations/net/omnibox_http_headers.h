// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_NET_OMNIBOX_HTTP_HEADERS_H_
#define COMPONENTS_VARIATIONS_NET_OMNIBOX_HTTP_HEADERS_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

class GURL;

namespace network {
struct ResourceRequest;
}

namespace variations {

COMPONENT_EXPORT(OMNIBOX_HTTP_HEADERS)
extern const char kOmniboxOnDeviceSuggestionsHeader[];

COMPONENT_EXPORT(OMNIBOX_HTTP_HEADERS)
BASE_DECLARE_FEATURE(kReportOmniboxOnDeviceSuggestionsHeader);

// Returns the value to set for the header. Returns an empty string if the
// provider is not explicitly enabled nor disabled via a field trial, i.e. the
// header is only set to a non-empty value during experimentation.
COMPONENT_EXPORT(OMNIBOX_HTTP_HEADERS)
std::string GetHeaderValue();

// Returns whether the header should be sent for this URL. The header is only
// sent to Google domains served over HTTPS.
COMPONENT_EXPORT(OMNIBOX_HTTP_HEADERS)
bool ShouldAppendHeader(const GURL& url);

// Adds an HTTP header reporting whether the Omnibox on-device search
// suggestions provider is enabled. Only reported if the provider is in a
// non-default state and if the |url| is a Google domain. This method is safe to
// call both from browser and from child processes.
COMPONENT_EXPORT(OMNIBOX_HTTP_HEADERS)
void AppendOmniboxOnDeviceSuggestionsHeaderIfNeeded(
    const GURL& url,
    network::ResourceRequest* request);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_NET_OMNIBOX_HTTP_HEADERS_H_
