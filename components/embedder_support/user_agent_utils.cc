// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/user_agent_utils.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/embedder_support/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/registry.h"
#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

namespace embedder_support {

namespace {

#if BUILDFLAG(IS_WIN)

// The registry key where the UniversalApiContract version value can be read
// from.
constexpr wchar_t kWindowsRuntimeWellKnownContractsRegKeyName[] =
    L"SOFTWARE\\Microsoft\\WindowsRuntime\\WellKnownContracts";

// Name of the UniversalApiContract registry.
constexpr wchar_t kUniversalApiContractName[] =
    L"Windows.Foundation.UniversalApiContract";

// There's a chance that access to the registry key that contains the
// UniversalApiContract Version will not be available in the future. After we
// confirm that our Windows version is RS5 or greater, it is best to have the
// default return value be the highest known version number at the time this
// code is submitted. If the UniversalApiContract registry key is no longer
// available, there will either be a new API introduced, or we will need
// to rely on querying the IsApiContractPresentByMajor function used by
// user_agent_utils_unittest.cc.
const int kHighestKnownUniversalApiContractVersion = 15;

int GetPreRS5UniversalApiContractVersion() {
  // This calls Kernel32Version() to get the real non-spoofable version (as
  // opposed to base::win::GetVersion() which as of writing this seems to return
  // different results depending on compatibility mode, and is spoofable).
  // See crbug.com/1404448.
  const base::win::Version version = base::win::OSInfo::Kernel32Version();
  if (version == base::win::Version::WIN10) {
    return 1;
  }
  if (version == base::win::Version::WIN10_TH2) {
    return 2;
  }
  if (version == base::win::Version::WIN10_RS1) {
    return 3;
  }
  if (version == base::win::Version::WIN10_RS2) {
    return 4;
  }
  if (version == base::win::Version::WIN10_RS3) {
    return 5;
  }
  if (version == base::win::Version::WIN10_RS4) {
    return 6;
  }
  // The list above should account for all Windows versions prior to
  // RS5.
  NOTREACHED_IN_MIGRATION();
  return 0;
}

// Returns the UniversalApiContract version number, which is available for
// Windows versions greater than RS5. Otherwise, returns 0.
const std::string& GetUniversalApiContractVersion() {
  // Do not use this for runtime environment detection logic. This method should
  // only be used to help populate the Sec-CH-UA-Platform client hint. If
  // authoring code that depends on a minimum API contract version being
  // available, you should instead leverage the OS's IsApiContractPresentByMajor
  // method.
  static const base::NoDestructor<std::string> universal_api_contract_version(
      [] {
        int major_version = 0;
        int minor_version = 0;
        if (base::win::OSInfo::Kernel32Version() <=
            base::win::Version::WIN10_RS4) {
          major_version = GetPreRS5UniversalApiContractVersion();
        } else {
          base::win::RegKey version_key(
              HKEY_LOCAL_MACHINE, kWindowsRuntimeWellKnownContractsRegKeyName,
              KEY_QUERY_VALUE | KEY_WOW64_64KEY);
          if (version_key.Valid()) {
            DWORD universal_api_contract_version = 0;
            LONG result = version_key.ReadValueDW(
                kUniversalApiContractName, &universal_api_contract_version);
            if (result == ERROR_SUCCESS) {
              major_version = HIWORD(universal_api_contract_version);
              minor_version = LOWORD(universal_api_contract_version);
            } else {
              major_version = kHighestKnownUniversalApiContractVersion;
            }
          } else {
            major_version = kHighestKnownUniversalApiContractVersion;
          }
        }
        // The major version of the contract is stored in the HIWORD, while the
        // minor version is stored in the LOWORD.
        return base::StrCat({base::NumberToString(major_version), ".",
                             base::NumberToString(minor_version), ".0"});
      }());
  return *universal_api_contract_version;
}

const std::string& GetWindowsPlatformVersion() {
  return GetUniversalApiContractVersion();
}
#endif  // BUILDFLAG(IS_WIN)

// Returns true if the user agent reduction should be forced (or prevented).
// TODO(crbug.com/1330890): Remove this method along with policy.
bool ShouldReduceUserAgentMinorVersion(
    UserAgentReductionEnterprisePolicyState user_agent_reduction) {
  return ((user_agent_reduction !=
               UserAgentReductionEnterprisePolicyState::kForceDisabled &&
           base::FeatureList::IsEnabled(
               blink::features::kReduceUserAgentMinorVersion)) ||
          user_agent_reduction ==
              UserAgentReductionEnterprisePolicyState::kForceEnabled);
}

// For desktop:
// Returns true if both kReduceUserAgentMinorVersionName and
// kReduceUserAgentPlatformOsCpu are enabled. It makes
// kReduceUserAgentPlatformOsCpu depend on kReduceUserAgentMinorVersionName.
//
// For android:
// Returns true if both kReduceUserAgentMinorVersionName and
// kReduceUserAgentAndroidVersionDeviceModel are enabled. It makes
// kReduceUserAgentAndroidVersionDeviceModel depend on
// kReduceUserAgentMinorVersionName.
//
// It helps us avoid introducing individual enterprise policy controls for
// sending unified platform for the user agent string.
bool ShouldSendUserAgentUnifiedPlatform(
    UserAgentReductionEnterprisePolicyState user_agent_reduction) {
#if BUILDFLAG(IS_ANDROID)
  return ShouldReduceUserAgentMinorVersion(user_agent_reduction) &&
         base::FeatureList::IsEnabled(
             blink::features::kReduceUserAgentAndroidVersionDeviceModel);
#else
  return ShouldReduceUserAgentMinorVersion(user_agent_reduction) &&
         base::FeatureList::IsEnabled(
             blink::features::kReduceUserAgentPlatformOsCpu) &&
         blink::features::kAllExceptLegacyWindowsPlatform.Get();
#endif
}

const blink::UserAgentBrandList GetUserAgentBrandList(
    const std::string& major_version,
    bool enable_updated_grease_by_policy,
    const std::string& full_version,
    blink::UserAgentBrandVersionType output_version_type) {
  int major_version_number;
  bool parse_result = base::StringToInt(major_version, &major_version_number);
  DCHECK(parse_result);
  std::optional<std::string> brand;
#if !BUILDFLAG(CHROMIUM_BRANDING)
  brand = version_info::GetProductName();
#endif
  std::optional<std::string> maybe_brand_override =
      base::GetFieldTrialParamValueByFeature(features::kGreaseUACH,
                                             "brand_override");
  std::optional<std::string> maybe_version_override =
      base::GetFieldTrialParamValueByFeature(features::kGreaseUACH,
                                             "version_override");
  if (maybe_brand_override->empty())
    maybe_brand_override = std::nullopt;
  if (maybe_version_override->empty())
    maybe_version_override = std::nullopt;

  std::string brand_version =
      output_version_type == blink::UserAgentBrandVersionType::kFullVersion
          ? full_version
          : major_version;

  return GenerateBrandVersionList(major_version_number, brand, brand_version,
                                  maybe_brand_override, maybe_version_override,
                                  enable_updated_grease_by_policy,
                                  output_version_type);
}

// Return UserAgentBrandList with the major version populated in the brand
// `version` value.
// TODO(crbug.com/1291612): Consolidate *MajorVersionList() methods by using
// GetVersionNumber()
const blink::UserAgentBrandList GetUserAgentBrandMajorVersionList(
    bool enable_updated_grease_by_policy) {
  return GetUserAgentBrandList(version_info::GetMajorVersionNumber(),
                               enable_updated_grease_by_policy,
                               std::string(version_info::GetVersionNumber()),
                               blink::UserAgentBrandVersionType::kMajorVersion);
}

// Return UserAgentBrandList with the full version populated in the brand
// `version` value.
// TODO(crbug.com/1291612): Consolidate *FullVersionList() methods by using
// GetVersionNumber()
blink::UserAgentBrandList GetUserAgentBrandFullVersionList(
    bool enable_updated_grease_by_policy) {
  return GetUserAgentBrandList(version_info::GetMajorVersionNumber(),
                               enable_updated_grease_by_policy,
                               std::string(version_info::GetVersionNumber()),
                               blink::UserAgentBrandVersionType::kFullVersion);
}

std::vector<std::string> GetFormFactorsClientHint(
    const blink::UserAgentMetadata& metadata,
    bool is_mobile) {
  // By default, use "Mobile" or "Desktop" depending on the `mobile` bit.
  std::vector<std::string> form_factors = {
      is_mobile ? blink::kMobileFormFactor : blink::kDesktopFormFactor};

  if (base::FeatureList::IsEnabled(blink::features::kClientHintsXRFormFactor)) {
    form_factors.push_back(blink::kXRFormFactor);
  }
  return form_factors;
}

}  // namespace

std::string GetProductAndVersion(
    UserAgentReductionEnterprisePolicyState user_agent_reduction) {
  return ShouldReduceUserAgentMinorVersion(user_agent_reduction)
             ? version_info::GetProductNameAndVersionForReducedUserAgent(
                   blink::features::kUserAgentFrozenBuildVersion.Get())
             : std::string(
                   version_info::GetProductNameAndVersionForUserAgent());
}

// Internal function to handle return the full or "reduced" user agent string,
// depending on the UserAgentReduction enterprise policy.
std::string GetUserAgentInternal(
    UserAgentReductionEnterprisePolicyState user_agent_reduction) {
  std::string product = GetProductAndVersion(user_agent_reduction);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kHeadless)) {
    product.insert(0, "Headless");
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMobileUserAgent))
    product += " Mobile";
#endif

  // In User-Agent reduction phase 5, only apply the <unifiedPlatform> to
  // desktop UA strings.
  // In User-Agent reduction phase 6, only apply the <unifiedPlatform> to
  // android UA strings.
  return ShouldSendUserAgentUnifiedPlatform(user_agent_reduction)
             ? content::BuildUnifiedPlatformUserAgentFromProduct(product)
             : content::BuildUserAgentFromProduct(product);
}

std::optional<std::string> GetUserAgentFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUserAgent)) {
    std::string ua = command_line->GetSwitchValueASCII(kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua)) {
      return ua;
    }
    LOG(WARNING) << "Ignored invalid value for flag --" << kUserAgent;
  }
  return std::nullopt;
}

std::string GetUserAgent(
    UserAgentReductionEnterprisePolicyState user_agent_reduction) {
  std::optional<std::string> custom_ua = GetUserAgentFromCommandLine();
  if (custom_ua.has_value()) {
    return custom_ua.value();
  }

  return GetUserAgentInternal(user_agent_reduction);
}

// Generate a pseudo-random permutation of the following brand/version pairs:
//   1. The base project (i.e. Chromium)
//   2. The browser brand, if available
//   3. A randomized string containing GREASE characters to ensure proper
//      header parsing, along with an arbitrarily low version to ensure proper
//      version checking.
blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    std::optional<std::string> brand,
    const std::string& version,
    std::optional<std::string> maybe_greasey_brand,
    std::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy,
    blink::UserAgentBrandVersionType output_version_type) {
  DCHECK_GE(seed, 0);
  const int npermutations = 6;  // 3!
  int permutation = seed % npermutations;

  // Pick a stable permutation seeded by major version number. any values here
  // and in order should be under three.
  const std::vector<std::vector<int>> orders{{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                             {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  const std::vector<int> order = orders[permutation];
  DCHECK_EQ(6u, orders.size());
  DCHECK_EQ(3u, order.size());

  blink::UserAgentBrandVersion greasey_bv = GetGreasedUserAgentBrandVersion(
      order, seed, maybe_greasey_brand, maybe_greasey_version,
      enable_updated_grease_by_policy, output_version_type);
  blink::UserAgentBrandVersion chromium_bv = {"Chromium", version};
  blink::UserAgentBrandList greased_brand_version_list(3);

  if (brand) {
    blink::UserAgentBrandVersion brand_bv = {brand.value(), version};

    greased_brand_version_list[order[0]] = greasey_bv;
    greased_brand_version_list[order[1]] = chromium_bv;
    greased_brand_version_list[order[2]] = brand_bv;
  } else {
    greased_brand_version_list[seed % 2] = greasey_bv;
    greased_brand_version_list[(seed + 1) % 2] = chromium_bv;

    // If left, the last element would make a blank "" at the end of the header.
    greased_brand_version_list.pop_back();
  }

  return greased_brand_version_list;
}

// Process greased overridden brand version which is either major version or
// full version, return the corresponding output version type.
blink::UserAgentBrandVersion GetProcessedGreasedBrandVersion(
    const std::string& greasey_brand,
    const std::string& greasey_version,
    blink::UserAgentBrandVersionType output_version_type) {
  std::string greasey_major_version;
  std::string greasey_full_version;
  base::Version version(greasey_version);
  DCHECK(version.IsValid());

  // If the greased overridden version is a significant version type:
  // * Major version: set the major version as the overridden version
  // * Full version number: extending the version number with ".0.0.0"
  // If the overridden version is full version format:
  // * Major version: set the major version to match significant version format
  // * Full version: set the full version as the overridden version
  // https://wicg.github.io/ua-client-hints/#user-agent-full-version
  if (version.components().size() > 1) {
    greasey_major_version = base::NumberToString(version.components()[0]);
    greasey_full_version = greasey_version;
  } else {
    greasey_major_version = greasey_version;
    greasey_full_version = base::StrCat({greasey_version, ".0.0.0"});
  }

  blink::UserAgentBrandVersion output_greasey_bv = {
      greasey_brand,
      output_version_type == blink::UserAgentBrandVersionType::kFullVersion
          ? greasey_full_version
          : greasey_major_version};
  return output_greasey_bv;
}

blink::UserAgentBrandVersion GetGreasedUserAgentBrandVersion(
    std::vector<int> permuted_order,
    int seed,
    std::optional<std::string> maybe_greasey_brand,
    std::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy,
    blink::UserAgentBrandVersionType output_version_type) {
  std::string greasey_brand;
  std::string greasey_version;
  // The updated algorithm is enabled by default, but we maintain the ability
  // to opt out of it either via Finch (setting updated_algorithm to false) or
  // via an enterprise policy escape hatch.
  if (enable_updated_grease_by_policy &&
      base::GetFieldTrialParamByFeatureAsBool(features::kGreaseUACH,
                                              "updated_algorithm", true)) {
    const std::vector<std::string> greasey_chars = {
        " ", "(", ":", "-", ".", "/", ")", ";", "=", "?", "_"};
    const std::vector<std::string> greased_versions = {"8", "99", "24"};
    // See the spec:
    // https://wicg.github.io/ua-client-hints/#create-arbitrary-brands-section
    greasey_brand = base::StrCat(
        {"Not", greasey_chars[(seed) % greasey_chars.size()], "A",
         greasey_chars[(seed + 1) % greasey_chars.size()], "Brand"});
    greasey_version = greased_versions[seed % greased_versions.size()];

    return GetProcessedGreasedBrandVersion(
        maybe_greasey_brand.value_or(greasey_brand),
        maybe_greasey_version.value_or(greasey_version), output_version_type);
  } else {
    const std::vector<std::string> greasey_chars = {" ", " ", ";"};
    greasey_brand = base::StrCat({greasey_chars[permuted_order[0]], "Not",
                                  greasey_chars[permuted_order[1]], "A",
                                  greasey_chars[permuted_order[2]], "Brand"});
    greasey_version = "99";

    // The old algorithm is held constant; it does not respond to experiment
    // overrides.
    return GetProcessedGreasedBrandVersion(greasey_brand, greasey_version,
                                           output_version_type);
  }
}

std::string GetPlatformForUAMetadata() {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40704421): This can be removed/re-refactored once we use
  // "macOS" by default
  return "macOS";
#elif BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40846294): The branding change to remove the space caused a
  // regression that's solved here. Ideally, we would just use the new OS name
  // without the space here too, but that needs a launch plan.
# if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "Chrome OS";
# else
  return "Chromium OS";
# endif
#else
  return std::string(version_info::GetOSType());
#endif
}

blink::UserAgentMetadata GetUserAgentMetadata(bool only_low_entropy_ch) {
  return GetUserAgentMetadata(nullptr, only_low_entropy_ch);
}

blink::UserAgentMetadata GetUserAgentMetadata(const PrefService* pref_service,
                                              bool only_low_entropy_ch) {
  blink::UserAgentMetadata metadata;

  bool enable_updated_grease_by_policy = true;
  // TODO(crbug.com/40838057): Remove this after M126 which deprecates the
  // policy.
  if (pref_service) {
    if (pref_service->HasPrefPath(
            policy::policy_prefs::kUserAgentClientHintsGREASEUpdateEnabled))
      enable_updated_grease_by_policy = pref_service->GetBoolean(
          policy::policy_prefs::kUserAgentClientHintsGREASEUpdateEnabled);
  }

  // Low entropy client hints.
  metadata.brand_version_list =
      GetUserAgentBrandMajorVersionList(enable_updated_grease_by_policy);
  metadata.mobile = false;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  metadata.mobile = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseMobileUserAgent);
#endif
  metadata.platform = GetPlatformForUAMetadata();

  // For users providing a valid user-agent override via the command line:
  // If kUACHOverrideBlank is enabled, set user-agent metadata with the
  // default blank values, otherwise return the default UserAgentMetadata values
  // to populate and send only the low entropy client hints.
  // Notes: Sending low entropy hints with empty values may cause requests being
  // blocked by web application firewall software, etc.
  std::optional<std::string> custom_ua = GetUserAgentFromCommandLine();
  if (custom_ua.has_value()) {
    return base::FeatureList::IsEnabled(blink::features::kUACHOverrideBlank)
               ? blink::UserAgentMetadata()
               : metadata;
  }

  if (only_low_entropy_ch) {
    return metadata;
  }

  // High entropy client hints.
  metadata.brand_full_version_list =
      GetUserAgentBrandFullVersionList(enable_updated_grease_by_policy);
  metadata.full_version = std::string(version_info::GetVersionNumber());
  metadata.architecture = content::GetCpuArchitecture();
  metadata.model = content::BuildModelInfo();
  metadata.form_factors = GetFormFactorsClientHint(metadata, metadata.mobile);

#if BUILDFLAG(IS_WIN)
  metadata.platform_version = GetWindowsPlatformVersion();
#else
  int32_t major, minor, bugfix = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  metadata.platform_version =
      base::StringPrintf("%d.%d.%d", major, minor, bugfix);
#endif
  metadata.architecture = content::GetCpuArchitecture();
  metadata.bitness = content::GetCpuBitness();
  metadata.wow64 = content::IsWoW64();

  return metadata;
}

#if BUILDFLAG(IS_ANDROID)
void SetDesktopUserAgentOverride(content::WebContents* web_contents,
                                 const blink::UserAgentMetadata& metadata,
                                 bool override_in_new_tabs) {
  const char kLinuxInfoStr[] = "X11; Linux x86_64";

  blink::UserAgentOverride spoofed_ua;
  spoofed_ua.ua_string_override = content::BuildUserAgentFromOSAndProduct(
      kLinuxInfoStr, GetProductAndVersion());
  spoofed_ua.ua_metadata_override = metadata;
  spoofed_ua.ua_metadata_override->platform = "Linux";
  spoofed_ua.ua_metadata_override->platform_version =
      std::string();  // match content::GetOSVersion(false) on Linux
  spoofed_ua.ua_metadata_override->model = std::string();
  spoofed_ua.ua_metadata_override->mobile = false;
  spoofed_ua.ua_metadata_override->form_factors =
      GetFormFactorsClientHint(metadata, /*is_mobile=*/false);
  // Match the above "CpuInfo" string, which is also the most common Linux
  // CPU architecture and bitness.`
  spoofed_ua.ua_metadata_override->architecture = "x86";
  spoofed_ua.ua_metadata_override->bitness = "64";
  spoofed_ua.ua_metadata_override->wow64 = false;

  web_contents->SetUserAgentOverride(spoofed_ua, override_in_new_tabs);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
int GetHighestKnownUniversalApiContractVersionForTesting() {
  return kHighestKnownUniversalApiContractVersion;
}
#endif  // BUILDFLAG(IS_WIN)

embedder_support::UserAgentReductionEnterprisePolicyState
GetUserAgentReductionFromPrefs(const PrefService* pref_service) {
  if (!pref_service->HasPrefPath(kReduceUserAgentMinorVersion))
    return UserAgentReductionEnterprisePolicyState::kDefault;
  switch (pref_service->GetInteger(kReduceUserAgentMinorVersion)) {
    case 1:
      return UserAgentReductionEnterprisePolicyState::kForceDisabled;
    case 2:
      return UserAgentReductionEnterprisePolicyState::kForceEnabled;
    case 0:
    default:
      return UserAgentReductionEnterprisePolicyState::kDefault;
  }
}

}  // namespace embedder_support
