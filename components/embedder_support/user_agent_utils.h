// Copyright 2021 The Chromium Authors
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

// TODO(crbug.com/40843535): Remove this enum along with policy.
enum class UserAgentReductionEnterprisePolicyState {
  kDefault = 0,
  kForceDisabled = 1,
  kForceEnabled = 2,
};

// Returns the product & version string.  Examples:
//   "Chrome/101.0.0.0"       - if UA reduction is enabled
//   "Chrome/101.0.4698.0"    - if UA reduction isn't enabled
// TODO(crbug.com/40212812): modify to accept an optional PrefService*.
std::string GetProductAndVersion(
    UserAgentReductionEnterprisePolicyState user_agent_reduction =
        UserAgentReductionEnterprisePolicyState::kDefault);

// Returns the full or "reduced" user agent string, depending on the following:
// 1) UserAgentReduction enterprise policy.
// 2) Reduce User-Agent reduction phase features.
// TODO(crbug.com/40212812): modify to accept an optional PrefService*.
std::string GetUserAgent(
    UserAgentReductionEnterprisePolicyState user_agent_reduction =
        UserAgentReductionEnterprisePolicyState::kDefault);

// Returns UserAgentMetadata per the default policy. This override is currently
// used in fuchsia and headless_shell, where the enterprise policy is not
// relevant.
// `only_low_entropy_ch` indicates whether only populate the low entropy client
// hints, the default is false.
blink::UserAgentMetadata GetUserAgentMetadata(bool only_low_entropy_ch = false);

// Return UserAgentMetadata, potentially overridden by policy.
// Note that this override is likely to be removed once an enterprise
// escape hatch is no longer needed. See https://crbug.com/1261908.
// `only_low_entropy_ch` indicates whether only populate the low entropy client
// hints.
blink::UserAgentMetadata GetUserAgentMetadata(const PrefService* local_state,
                                              bool only_low_entropy_ch = false);

// Return UserAgentBrandList based on the expected output version type.
blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    std::optional<std::string> brand,
    const std::string& version,
    std::optional<std::string> maybe_greasey_brand,
    std::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy,
    blink::UserAgentBrandVersionType output_version_type);

// Return greased UserAgentBrandVersion to prevent assumptions about the
// current values being baked into implementations. See
// https://wicg.github.io/ua-client-hints/#create-arbitrary-brands-section.
blink::UserAgentBrandVersion GetGreasedUserAgentBrandVersion(
    std::vector<int> permuted_order,
    int seed,
    std::optional<std::string> maybe_greasey_brand,
    std::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy,
    blink::UserAgentBrandVersionType output_version_type);

#if BUILDFLAG(IS_ANDROID)
// This sets a user agent string to simulate a desktop user agent on mobile.
// If |override_in_new_tabs| is true, and the first navigation in the tab is
// renderer initiated, then is-overriding-user-agent is set to true for the
// NavigationEntry.
void SetDesktopUserAgentOverride(content::WebContents* web_contents,
                                 const blink::UserAgentMetadata& metadata,
                                 bool override_in_new_tabs);
#endif

#if BUILDFLAG(IS_WIN)
int GetHighestKnownUniversalApiContractVersionForTesting();
#endif  // BUILDFLAG(IS_WIN)

// Returns the UserAgentReductionEnterprisePolicyState enum value corresponding
// to the provided integer policy value for UserAgentReduction.
// TODO(crbug.com/40843535): Remove this function with policy.
embedder_support::UserAgentReductionEnterprisePolicyState
GetUserAgentReductionFromPrefs(const PrefService* pref_service);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_
