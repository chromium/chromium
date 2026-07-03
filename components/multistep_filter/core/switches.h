// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_SWITCHES_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_SWITCHES_H_

namespace multistep_filter::switches {

// Overrides the base URL for the `SiteAutomationIndexServer` Server APIs.
inline constexpr char kMultistepFilterIndexServerApiBaseUrl[] =
    "multistep-filter-index-server-api-base-url";

// Allows HTTP URLs for extraction and suggestions during browser testing.
inline constexpr char kMultistepFilterAllowHttpForTesting[] =
    "multistep-filter-allow-http-for-testing";

// Enables the use of the Optimization Guide annotation index client.
inline constexpr char kMultistepFilterOptimizationGuide[] =
    "multistep-filter-optimization-guide";

}  // namespace multistep_filter::switches

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_SWITCHES_H_
