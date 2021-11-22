// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/user_agent_utils.h"

#include "base/command_line.h"
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

constexpr char kMajorVersion100[] = "100";

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
std::string GetUniversalApiContractVersion() {
  // Do not use this for runtime environment detection logic. This method should
  // only be used to help populate the Sec-CH-UA-Platform client hint. If
  // authoring code that depends on a minimum API contract version being
  // available, you should instead leverage the OS's IsApiContractPresentByMajor
  // method.
  int major_version = 0;
  int minor_version = 0;
  if (base::win::GetVersion() >= base::win::Version::WIN10) {
    if (base::win::GetVersion() <= base::win::Version::WIN10_RS4) {
      major_version = GetPreRS5UniversalApiContractVersion();
    } else {
      base::win::RegKey version_key(HKEY_LOCAL_MACHINE,
                                    kWindowsRuntimeWellKnownContractsRegKeyName,
                                    KEY_QUERY_VALUE | KEY_WOW64_64KEY);
      if (version_key.Valid()) {
        DWORD universal_api_contract_version = 0;
        LONG result = version_key.ReadValueDW(kUniversalApiContractName,
                                              &universal_api_contract_version);
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
}

#endif  // defined(OS_WIN)

const std::string& GetM100VersionNumber() {
  static const base::NoDestructor<std::string> m100_version_number([] {
    base::Version version(version_info::GetVersionNumber());
    std::string version_str(kMajorVersion100);
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

const blink::UserAgentBrandList GetUserAgentBrandList(
    const std::string& major_version) {
  int major_version_number;
  base::StringToInt(major_version, &major_version_number);
  absl::optional<std::string> brand;
#if !BUILDFLAG(CHROMIUM_BRANDING)
  brand = version_info::GetProductName();
#endif
  absl::optional<std::string> maybe_param_override =
      base::GetFieldTrialParamValueByFeature(features::kGreaseUACH,
                                             "brand_override");
  if (maybe_param_override->empty())
    maybe_param_override = absl::nullopt;

  return GenerateBrandVersionList(major_version_number, brand, major_version,
                                  maybe_param_override);
}

const blink::UserAgentBrandList& GetUserAgentBrandList() {
  static const base::NoDestructor<blink::UserAgentBrandList> brand_list(
      GetUserAgentBrandList(version_info::GetMajorVersionNumber()));
  return *brand_list;
}

const blink::UserAgentBrandList& GetForcedM100UserAgentBrandList() {
  static const base::NoDestructor<blink::UserAgentBrandList> brand_list(
      GetUserAgentBrandList(kMajorVersion100));
  return *brand_list;
}

const blink::UserAgentBrandList& GetBrandVersionList() {
  if (base::FeatureList::IsEnabled(
          blink::features::kForceMajorVersion100InUserAgent))
    return GetForcedM100UserAgentBrandList();

  return GetUserAgentBrandList();
}

}  // namespace

std::string GetProduct() {
  if (base::FeatureList::IsEnabled(
          blink::features::kForceMajorVersion100InUserAgent))
    return "Chrome/" + GetM100VersionNumber();

  return version_info::GetProductNameAndVersionForUserAgent();
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

  std::string product = GetProduct();
#if defined(OS_ANDROID)
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return content::BuildUserAgentFromProduct(product);
}

std::string GetReducedUserAgent() {
  return content::GetReducedUserAgent(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMobileUserAgent),
      base::FeatureList::IsEnabled(
          blink::features::kForceMajorVersion100InUserAgent)
          ? kMajorVersion100
          : version_info::GetMajorVersionNumber());
}

// Generate a pseudo-random permutation of the following brand/version pairs:
//   1. The base project (i.e. Chromium)
//   2. The browser brand, if available
//   3. A randomized string containing escaped characters to ensure proper
//      header parsing, along with an arbitrarily low version to ensure proper
//      version checking.
blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    absl::optional<std::string> brand,
    std::string major_version,
    absl::optional<std::string> maybe_greasey_brand) {
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

  // Previous values for indexes 0 and 1 were '\' and '"', temporarily removed
  // because of compat issues
  const std::vector<std::string> escaped_chars = {" ", " ", ";"};
  std::string greasey_brand =
      base::StrCat({escaped_chars[order[0]], "Not", escaped_chars[order[1]],
                    "A", escaped_chars[order[2]], "Brand"});

  blink::UserAgentBrandVersion greasey_bv = {
      maybe_greasey_brand.value_or(greasey_brand), "99"};
  blink::UserAgentBrandVersion chromium_bv = {"Chromium", major_version};

  blink::UserAgentBrandList greased_brand_version_list(3);

  if (brand) {
    blink::UserAgentBrandVersion brand_bv = {brand.value(), major_version};

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
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list = GetBrandVersionList();
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
}

#if defined(OS_ANDROID)
void SetDesktopUserAgentOverride(content::WebContents* web_contents,
                                 const blink::UserAgentMetadata& metadata,
                                 bool override_in_new_tabs) {
  const char kLinuxInfoStr[] = "X11; Linux x86_64";

  blink::UserAgentOverride spoofed_ua;
  spoofed_ua.ua_string_override =
      content::BuildUserAgentFromOSAndProduct(kLinuxInfoStr, GetProduct());
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
