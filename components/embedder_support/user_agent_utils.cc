// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/user_agent_utils.h"

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

#if defined(OS_WIN)
#include <windows.h>

#include "base/win/registry.h"
#include "base/win/windows_version.h"
#endif  // defined(OS_WIN)

namespace embedder_support {

namespace {

constexpr char kVersion100[] = "100";

#if defined(OS_WIN)

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
const int kHighestKnownUniversalApiContractVersion = 14;

int GetPreRS5UniversalApiContractVersion() {
  if (base::win::GetVersion() == base::win::Version::WIN10)
    return 1;
  if (base::win::GetVersion() == base::win::Version::WIN10_TH2)
    return 2;
  if (base::win::GetVersion() == base::win::Version::WIN10_RS1)
    return 3;
  if (base::win::GetVersion() == base::win::Version::WIN10_RS2)
    return 4;
  if (base::win::GetVersion() == base::win::Version::WIN10_RS3)
    return 5;
  if (base::win::GetVersion() == base::win::Version::WIN10_RS4)
    return 6;
  // The list above should account for all Windows versions prior to
  // RS5.
  NOTREACHED();
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
        if (base::win::GetVersion() >= base::win::Version::WIN10) {
          if (base::win::GetVersion() <= base::win::Version::WIN10_RS4) {
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
        }
        // The major version of the contract is stored in the HIWORD, while the
        // minor version is stored in the LOWORD.
        return base::StrCat({base::NumberToString(major_version), ".",
                             base::NumberToString(minor_version), ".0"});
      }());
  return *universal_api_contract_version;
}

#endif  // defined(OS_WIN)

const std::string& GetM100VersionNumber() {
  static const base::NoDestructor<std::string> m100_version_number([] {
    base::Version version(version_info::GetVersionNumber());
    std::string version_str(kVersion100);
    const std::vector<uint32_t>& components = version.components();
    // Rest of the version string remains the same.
    for (size_t i = 1; i < components.size(); ++i) {
      version_str.append(".");
      version_str.append(base::NumberToString(components[i]));
    }
    return version_str;
  }());
  return *m100_version_number;
}

const std::string& GetM100InMinorVersionNumber() {
  static const base::NoDestructor<std::string> m100_version_number([] {
    base::Version version(version_info::GetVersionNumber());
    std::string version_str;
    const std::vector<uint32_t>& components = version.components();
    // Rest of the version string remains the same.
    for (size_t i = 0; i < components.size(); ++i) {
      if (i > 0) {
        version_str.append(".");
      }
      if (i == 1) {
        // Populate "100" for the minor version.
        version_str.append(kVersion100);
      } else {
        version_str.append(base::NumberToString(components[i]));
      }
    }
    return version_str;
  }());
  return *m100_version_number;
}

const blink::UserAgentBrandList GetUserAgentBrandList(
    const std::string& major_version,
    bool enable_updated_grease_by_policy,
    const std::string& full_version,
    blink::UserAgentBrandVersionType output_version_type) {
  int major_version_number = 0;
  DCHECK(base::StringToInt(major_version, &major_version_number));
  absl::optional<std::string> brand;
#if !BUILDFLAG(CHROMIUM_BRANDING)
  brand = version_info::GetProductName();
#endif
  absl::optional<std::string> maybe_brand_override =
      base::GetFieldTrialParamValueByFeature(features::kGreaseUACH,
                                             "brand_override");
  absl::optional<std::string> maybe_version_override =
      base::GetFieldTrialParamValueByFeature(features::kGreaseUACH,
                                             "version_override");
  if (maybe_brand_override->empty())
    maybe_brand_override = absl::nullopt;
  if (maybe_version_override->empty())
    maybe_version_override = absl::nullopt;

  std::string brand_version =
      output_version_type == blink::UserAgentBrandVersionType::kFullVersion
          ? full_version
          : major_version;

  return GenerateBrandVersionList(major_version_number, brand, brand_version,
                                  maybe_brand_override, maybe_version_override,
                                  enable_updated_grease_by_policy,
                                  output_version_type);
}

const blink::UserAgentBrandList GetUserAgentBrandMajorVersionList(
    bool enable_updated_grease_by_policy) {
  return GetUserAgentBrandList(version_info::GetMajorVersionNumber(),
                               enable_updated_grease_by_policy,
                               version_info::GetVersionNumber(),
                               blink::UserAgentBrandVersionType::kMajorVersion);
}

blink::UserAgentBrandList GetForcedM100UserAgentBrandMajorVersionList(
    bool enable_updated_grease_by_policy) {
  return GetUserAgentBrandList(kVersion100, enable_updated_grease_by_policy,
                               GetM100VersionNumber(),
                               blink::UserAgentBrandVersionType::kMajorVersion);
}

blink::UserAgentBrandList GetUserAgentBrandFullVersionList(
    bool enable_updated_grease_by_policy) {
  return GetUserAgentBrandList(version_info::GetMajorVersionNumber(),
                               enable_updated_grease_by_policy,
                               version_info::GetVersionNumber(),
                               blink::UserAgentBrandVersionType::kFullVersion);
}

blink::UserAgentBrandList GetForcedM100UserAgentBrandFullVersionList(
    bool enable_updated_grease_by_policy) {
  return GetUserAgentBrandList(kVersion100, enable_updated_grease_by_policy,
                               GetM100VersionNumber(),
                               blink::UserAgentBrandVersionType::kFullVersion);
}

// Return UserAgentBrandList with the major version populated in the brand
// `version` value.
blink::UserAgentBrandList GetBrandMajorVersionList(
    bool enable_updated_grease_by_policy) {
  if (base::FeatureList::IsEnabled(
          blink::features::kForceMajorVersion100InUserAgent))
    return GetForcedM100UserAgentBrandMajorVersionList(
        enable_updated_grease_by_policy);

  return GetUserAgentBrandMajorVersionList(enable_updated_grease_by_policy);
}

// Return UserAgentBrandList with the full version populated in the brand
// `version` value.
blink::UserAgentBrandList GetBrandFullVersionList(
    bool enable_updated_grease_by_policy) {
  if (base::FeatureList::IsEnabled(
          blink::features::kForceMajorVersion100InUserAgent))
    return GetForcedM100UserAgentBrandFullVersionList(
        enable_updated_grease_by_policy);

  return GetUserAgentBrandFullVersionList(enable_updated_grease_by_policy);
}

std::string GetProduct(const bool allow_version_override) {
  if (allow_version_override &&
      base::FeatureList::IsEnabled(
          blink::features::kForceMajorVersion100InUserAgent))
    return "Chrome/" + GetM100VersionNumber();
  if (allow_version_override &&
      base::FeatureList::IsEnabled(
          blink::features::kForceMinorVersion100InUserAgent))
    return "Chrome/" + GetM100InMinorVersionNumber();

  return version_info::GetProductNameAndVersionForUserAgent();
}

}  // namespace

std::string GetProduct() {
  return GetProduct(/*allow_version_override=*/false);
}

std::string GetUserAgent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUserAgent)) {
    std::string ua = command_line->GetSwitchValueASCII(kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      return ua;
    LOG(WARNING) << "Ignored invalid value for flag --" << kUserAgent;
  }

  if (base::FeatureList::IsEnabled(blink::features::kReduceUserAgent))
    return GetReducedUserAgent();

  return GetFullUserAgent();
}

std::string GetReducedUserAgent() {
  return content::GetReducedUserAgent(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMobileUserAgent),
      base::FeatureList::IsEnabled(
          blink::features::kForceMajorVersion100InUserAgent)
          ? kVersion100
          : version_info::GetMajorVersionNumber());
}

std::string GetFullUserAgent() {
  std::string product = GetProduct(/*allow_version_override=*/true);
#if defined(OS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return content::BuildUserAgentFromProduct(product);
}

// Generate a pseudo-random permutation of the following brand/version pairs:
//   1. The base project (i.e. Chromium)
//   2. The browser brand, if available
//   3. A randomized string containing GREASE characters to ensure proper
//      header parsing, along with an arbitrarily low version to ensure proper
//      version checking.
blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    absl::optional<std::string> brand,
    const std::string& version,
    absl::optional<std::string> maybe_greasey_brand,
    absl::optional<std::string> maybe_greasey_version,
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
    absl::optional<std::string> maybe_greasey_brand,
    absl::optional<std::string> maybe_greasey_version,
    bool enable_updated_grease_by_policy,
    blink::UserAgentBrandVersionType output_version_type) {
  std::string greasey_brand;
  std::string greasey_version;
  if (enable_updated_grease_by_policy &&
      base::GetFieldTrialParamByFeatureAsBool(features::kGreaseUACH,
                                              "updated_algorithm", false)) {
    const std::vector<std::string> greasey_chars = {
        " ", "(", ":", "-", ".", "/", ")", ";", "=", "?", "_"};
    const std::vector<std::string> greased_versions = {"8", "99", "24"};
    // The spec disallows a leading or trailing space, so ensuring the first
    // char isn't index 0. See the spec:
    // https://wicg.github.io/ua-client-hints/#create-arbitrary-brands-section
    greasey_brand = base::StrCat(
        {greasey_chars[(seed % (greasey_chars.size() - 1)) + 1], "Not",
         greasey_chars[(seed + 1) % greasey_chars.size()], "A",
         greasey_chars[(seed + 2) % greasey_chars.size()], "Brand"});
    greasey_version = greased_versions[seed % greased_versions.size()];
  } else {
    const std::vector<std::string> greasey_chars = {" ", " ", ";"};
    greasey_brand = base::StrCat({greasey_chars[permuted_order[0]], "Not",
                                  greasey_chars[permuted_order[1]], "A",
                                  greasey_chars[permuted_order[2]], "Brand"});
    greasey_version = "99";
  }

  return GetProcessedGreasedBrandVersion(
      maybe_greasey_brand.value_or(greasey_brand),
      maybe_greasey_version.value_or(greasey_version), output_version_type);
}
// TODO(crbug.com/1103047): This can be removed/re-refactored once we use
// "macOS" by default
std::string GetPlatformForUAMetadata() {
#if defined(OS_MAC)
  return "macOS";
#else
  return version_info::GetOSType();
#endif
}

blink::UserAgentMetadata GetUserAgentMetadata() {
  return GetUserAgentMetadata(nullptr);
}

blink::UserAgentMetadata GetUserAgentMetadata(PrefService* pref_service) {
  blink::UserAgentMetadata metadata;
  bool enable_updated_grease_by_policy = true;
  if (pref_service &&
      pref_service->HasPrefPath(
          policy::policy_prefs::kUserAgentClientHintsGREASEUpdateEnabled)) {
    enable_updated_grease_by_policy = pref_service->GetBoolean(
        policy::policy_prefs::kUserAgentClientHintsGREASEUpdateEnabled);
  }
  metadata.brand_version_list =
      GetBrandMajorVersionList(enable_updated_grease_by_policy);
  metadata.brand_full_version_list =
      GetBrandFullVersionList(enable_updated_grease_by_policy);
  metadata.full_version = base::FeatureList::IsEnabled(
                              blink::features::kForceMajorVersion100InUserAgent)
                              ? GetM100VersionNumber()
                              : version_info::GetVersionNumber();
  metadata.platform = GetPlatformForUAMetadata();
  metadata.architecture = content::GetLowEntropyCpuArchitecture();
  metadata.model = content::BuildModelInfo();
  metadata.mobile = false;
#if defined(OS_ANDROID)
  metadata.mobile = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseMobileUserAgent);
#endif

#if defined(OS_WIN)
  metadata.platform_version = GetUniversalApiContractVersion();
#else
  int32_t major, minor, bugfix = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  metadata.platform_version =
      base::StringPrintf("%d.%d.%d", major, minor, bugfix);
#endif

  // These methods use the same information as the User-Agent string, but are
  // "low entropy" in that they reduce the number of options for output to a
  // set number. For more information, see the respective headers.
  metadata.architecture = content::GetLowEntropyCpuArchitecture();
  metadata.bitness = content::GetLowEntropyCpuBitness();

  return metadata;
}  // namespace embedder_support

#if defined(OS_ANDROID)
void SetDesktopUserAgentOverride(content::WebContents* web_contents,
                                 const blink::UserAgentMetadata& metadata,
                                 bool override_in_new_tabs) {
  const char kLinuxInfoStr[] = "X11; Linux x86_64";

  blink::UserAgentOverride spoofed_ua;
  spoofed_ua.ua_string_override = content::BuildUserAgentFromOSAndProduct(
      kLinuxInfoStr, GetProduct(/*allow_version_override=*/true));
  spoofed_ua.ua_metadata_override = metadata;
  spoofed_ua.ua_metadata_override->platform = "Linux";
  spoofed_ua.ua_metadata_override->platform_version =
      std::string();  // match content::GetOSVersion(false) on Linux
  spoofed_ua.ua_metadata_override->model = std::string();
  spoofed_ua.ua_metadata_override->mobile = false;
  // Match the above "CpuInfo" string, which is also the most common Linux
  // CPU architecture and bitness.`
  spoofed_ua.ua_metadata_override->architecture = "x86";
  spoofed_ua.ua_metadata_override->bitness = "64";

  web_contents->SetUserAgentOverride(spoofed_ua, override_in_new_tabs);
}
#endif  // OS_ANDROID

#if defined(OS_WIN)
int GetHighestKnownUniversalApiContractVersionForTesting() {
  return kHighestKnownUniversalApiContractVersion;
}
#endif  // defined(OS_WIN)

}  // namespace embedder_support
