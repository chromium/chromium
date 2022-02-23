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

// TODO(crbug.com/1291612): Move this enum definition to
// chrome/browser/chrome_content_browser_client.h
// TODO(crbug.com/1290820): Remove this enum along with policy.
enum ForceMajorVersionToMinorPosition {
  kDefault = 0,
  kForceDisabled = 1,
  kForceEnabled = 2,
};

struct UserAgentOptions {
  bool force_major_version_100 = false;
  ForceMajorVersionToMinorPosition force_major_to_minor = kDefault;
};

// Returns the product & version string.  Examples:
//   "Chrome/101.0.0.0"    -  if UA reduction is enabled
//   "Chrome/101.0.4698.0" -  if UA reduction is not enabled
std::string GetProductAndVersion(
    ForceMajorVersionToMinorPosition force_major_to_minor = kDefault);

// Returns the user agent string for Chrome.
// TODO(crbug.com/1291612): modify to accept an optional PrefService*.
std::string GetFullUserAgent(
    ForceMajorVersionToMinorPosition force_major_to_minor = kDefault);

// Returns the reduced user agent string for Chrome.
// TODO(crbug.com/1291612): modify to accept an optional PrefService*.
std::string GetReducedUserAgent(
    ForceMajorVersionToMinorPosition force_major_to_minor = kDefault);

// Returns the full or "reduced" user agent string, depending on the
// UserAgentReduction enterprise policy and blink::features::kReduceUserAgent
// TODO(crbug.com/1291612): modify to accept an optional PrefService*.
std::string GetUserAgent(
    ForceMajorVersionToMinorPosition force_major_to_minor = kDefault);

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

// Returns the ForcemajorVersionToMinorPosition enum value corresponding to
// the provided integer policy value for ForceMajorVersionToMinorPosition.
// TODO(crbug.com/1290820): Remove this function with policy.
embedder_support::ForceMajorVersionToMinorPosition GetMajorToMinorFromPrefs(
    PrefService* pref_service);

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_
