// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_

#include <string>

#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace blink {
struct UserAgentMetadata;
}

namespace content {
class WebContents;
}

namespace embedder_support {

// Returns the product used in building the user-agent.
std::string GetProduct();

// Returns the user agent string for Chrome. If the ReduceUserAgent
// feature is enabled, this will return |GetReducedUserAgent|
std::string GetUserAgent();

// Returns the reduced user agent string for Chrome.
std::string GetReducedUserAgent();

// Returns UserAgentMetadata per the default policy.
// This override is currently used in fuchsia, where the enterprise policy
// is not relevant.
blink::UserAgentMetadata GetUserAgentMetadata();

// Return UserAgentMetadata, potentially overridden by policy.
// Note that this override is likely to be removed once an enterprise
// escape hatch is no longer needed. See https://crbug.com/1261908.
blink::UserAgentMetadata GetUserAgentMetadata(PrefService* local_state);

blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    absl::optional<std::string> brand,
    std::string major_version,
    absl::optional<std::string> maybe_greasey_brand,
    absl::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy);

blink::UserAgentBrandVersion GetGreasedUserAgentBrandVersion(
    std::vector<int> permuted_order,
    int seed,
    absl::optional<std::string> maybe_greasey_brand,
    absl::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy);

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
