// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_

#include <string>

#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/common/user_agent/user_agent_brand_version_type.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace blink {
struct UserAgentMetadata;
}

namespace content {
class WebContents;
}

namespace embedder_support {

// Returns the product string, e.g. "Chrome/98.0.4521.0".  It's possible to have
// a mismatch between the product's version number and the version number in the
// User-Agent string, if there are flag-enabled overrides.
std::string GetProduct();

// Returns the user agent string for Chrome.
std::string GetFullUserAgent();

// Returns the reduced user agent string for Chrome.
std::string GetReducedUserAgent();

// Returns the full or "reduced" user agent string, depending on the
// UserAgentReduction enterprise policy and blink::features::kReduceUserAgent
std::string GetUserAgent();

// Returns UserAgentMetadata per the default policy.
// This override is currently used in fuchsia, where the enterprise policy
// is not relevant.
blink::UserAgentMetadata GetUserAgentMetadata();

// Return UserAgentMetadata, potentially overridden by policy.
// Note that this override is likely to be removed once an enterprise
// escape hatch is no longer needed. See https://crbug.com/1261908.
blink::UserAgentMetadata GetUserAgentMetadata(PrefService* local_state);

// Return UserAgentBrandList based on the expected output version type.
blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    absl::optional<std::string> brand,
    const std::string& version,
    absl::optional<std::string> maybe_greasey_brand,
    absl::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy,
    blink::UserAgentBrandVersionType output_version_type);

// Return greased UserAgentBrandVersion to prevent assumptions about the
// current values being baked into implementations. See
// https://wicg.github.io/ua-client-hints/#create-arbitrary-brands-section.
blink::UserAgentBrandVersion GetGreasedUserAgentBrandVersion(
    std::vector<int> permuted_order,
    int seed,
    absl::optional<std::string> maybe_greasey_brand,
    absl::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy,
    blink::UserAgentBrandVersionType output_version_type);

#if defined(OS_ANDROID)
// This sets a user agent string to simulate a desktop user agent on mobile.
// If |override_in_new_tabs| is true, and the first navigation in the tab is
// renderer initiated, then is-overriding-user-agent is set to true for the
// NavigationEntry.
void SetDesktopUserAgentOverride(content::WebContents* web_contents,
                                 const blink::UserAgentMetadata& metadata,
                                 bool override_in_new_tabs);
#endif

#if defined(OS_WIN)
int GetHighestKnownUniversalApiContractVersionForTesting();
#endif  // defined(OS_WIN)

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_
