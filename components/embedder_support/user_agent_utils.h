// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_

#include <optional>
#include <string>

#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/common/user_agent/user_agent_brand_version_type.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace blink {
struct UserAgentMetadata;
}

namespace embedder_support {

enum class IncludeAndroidBuildNumber { Include, Exclude };
enum class IncludeAndroidModel { Include, Exclude };

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

// Returns a user agent string passed via the kUserAgent command-line argument
// when it is valid, or std::nullopt if it is not valid.
std::optional<std::string> GetUserAgentFromCommandLine();

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

// Returns a list of form-factors compliant with
// https://wicg.github.io/ua-client-hints/#sec-ch-ua-form-factors.
std::vector<std::string> GetFormFactorsClientHint(
    const blink::UserAgentMetadata& metadata,
    bool is_mobile);

// Return UserAgentBrandList based on the expected output version type.
// Only use when adding additional brand version pair and overriding the default
// product brand version, otherwise prefer to
// GetUserAgentBrandFullVersionList/GetUserAgentBrandMajorVersionList.
blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    std::optional<std::string> brand,
    const std::string& version,
    blink::UserAgentBrandVersionType output_version_type,
    std::optional<blink::UserAgentBrandVersion> additional_brand_version =
        std::nullopt);

// Return UserAgentBrandList with full versions based on the additional brand
// version list if provided. It generates a pseudo-random permutation of the
// following brand/full_version pairs:
//   1. The base project (i.e. Chromium)
//   2. The browser brand, if available
//   3. A randomized string containing GREASE characters to ensure proper
//      header parsing, along with an arbitrarily low version to ensure proper
//      version checking.
//   4. Additional brand/full_version pairs.
const blink::UserAgentBrandList GetUserAgentBrandFullVersionList(
    std::optional<blink::UserAgentBrandVersion> additional_brand_version =
        std::nullopt);

// Return UserAgentBrandList with major versions based on the additional brand
// version list if provided. It generates a pseudo-random permutation of the
// following brand/major_version pairs:
//   1. The base project (i.e. Chromium)
//   2. The browser brand, if available
//   3. A randomized string containing GREASE characters to ensure proper
//      header parsing, along with an arbitrarily low version to ensure proper
//      version checking.
//   4. Additional brand/major_version pairs.
const blink::UserAgentBrandList GetUserAgentBrandMajorVersionList(
    std::optional<blink::UserAgentBrandVersion> additional_brand_version =
        std::nullopt);

// Return greased UserAgentBrandVersion to prevent assumptions about the
// current values being baked into implementations. See
// https://wicg.github.io/ua-client-hints/#create-arbitrary-brands-section.
blink::UserAgentBrandVersion GetGreasedUserAgentBrandVersion(
    int seed,
    blink::UserAgentBrandVersionType output_version_type);

#if BUILDFLAG(IS_WIN)
int GetHighestKnownUniversalApiContractVersionForTesting();
#endif  // BUILDFLAG(IS_WIN)

// Returns the UserAgentReductionEnterprisePolicyState enum value corresponding
// to the provided integer policy value for UserAgentReduction.
// TODO(crbug.com/40843535): Remove this function with policy.
UserAgentReductionEnterprisePolicyState GetUserAgentReductionFromPrefs(
    const PrefService* pref_service);

// Returns the (incorrectly named, for historical reasons) WebKit version, in
// the form "major.minor (@chromium_git_revision)".
std::string GetWebKitVersion();

std::string GetChromiumGitRevision();

// Returns the CPU architecture in Windows/Mac/POSIX/Fuchsia and the empty
// string on Android or if unknown.
std::string GetCpuArchitecture();

// Returns the CPU bitness in Windows/Mac/POSIX/Fuchsia and the empty string on
// Android.
std::string GetCpuBitness();

// We may also build the same User-agent compatible string describing OS and CPU
// type by providing our own |os_version| and |cpu_type|. This is primarily
// useful in testing.
std::string BuildOSCpuInfoFromOSVersionAndCpuType(const std::string& os_version,
                                                  const std::string& cpu_type);

// TODO(crbug.com/40200617): Remove this after user agent reduction phase 5 and
// --force-major-version-to-minor is removed.
// Return the <unifiedPlatform> token of a reduced User-Agent header.
std::string GetUnifiedPlatformForTesting();

// Helper function to generate a full user agent string from a short
// product name.
std::string BuildUserAgentFromProduct(const std::string& product);

// Helper function to generate a reduced user agent string with unified
// platform from a given product name.
std::string BuildUnifiedPlatformUserAgentFromProduct(
    const std::string& product);

// Returns the model information. Returns a blank string if not on Android or
// if on a codenamed (i.e. not a release) build of an Android.
std::string BuildModelInfo();

#if BUILDFLAG(IS_ANDROID)
// Helper function to generate a full user agent string given a short
// product name and some extra text to be added to the OS info.
// This is currently only used for Android Web View.
std::string BuildUserAgentFromProductAndExtraOSInfo(
    const std::string& product,
    const std::string& extra_os_info,
    IncludeAndroidBuildNumber include_android_build_number);

// Helper function to generate a reduced user agent string with unified
// platform from a given product name and extra os information.
std::string BuildUnifiedPlatformUAFromProductAndExtraOs(
    const std::string& product,
    const std::string& extra_os_info);

// Helper function to generate just the OS info.
std::string GetAndroidOSInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model);
#endif

// Builds a full user agent string given a string describing the OS and a
// product name.
std::string BuildUserAgentFromOSAndProduct(const std::string& os_info,
                                           const std::string& product);

// Returns true if the binary was built in 32-bit mode and is running on 64-bit
// Windows; returns false otherwise.
bool IsWoW64();

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_USER_AGENT_UTILS_H_
