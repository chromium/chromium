// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_SWITCHES_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_SWITCHES_H_

namespace multistep_filter::switches {

// Allows HTTP URLs for extraction and suggestions during browser testing.
inline constexpr char kMultistepFilterAllowHttpForTesting[] =
    "multistep-filter-allow-http-for-testing";

// Bypass the capability checks for local testing.
inline constexpr char kMultistepFilterBypassCapabilityCheck[] =
    "multistep-filter-bypass-capability-check";

// Tag the browser instance as running evals to filter out UMA metrics.
inline constexpr char kMultistepFilterEvals[] = "multistep-filter-evals";

}  // namespace multistep_filter::switches

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_SWITCHES_H_
