// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/user_agent_utils.h"

#include <stdint.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/util/chromium_git_revision.h"
#include "components/embedder_support/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/registry.h"
#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
#include "ui/base/device_form_factor.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include <sys/utsname.h>
#endif

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
const int kHighestKnownUniversalApiContractVersion = 19;

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
  NOTREACHED();
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
             blink::features::kReduceUserAgentPlatformOsCpu);
#endif
}

const blink::UserAgentBrandList GetUserAgentBrandList(
    const std::string& major_version,
    const std::string& full_version,
    blink::UserAgentBrandVersionType output_version_type,
    std::optional<blink::UserAgentBrandVersion> additional_brand_version) {
  int major_version_number;
  bool parse_result = base::StringToInt(major_version, &major_version_number);
  DCHECK(parse_result);
  std::optional<std::string> brand;
#if !BUILDFLAG(CHROMIUM_BRANDING)
  brand = version_info::GetProductName();
#endif

  std::string brand_version =
      output_version_type == blink::UserAgentBrandVersionType::kFullVersion
          ? full_version
          : major_version;

  return GenerateBrandVersionList(major_version_number, brand, brand_version,
                                  output_version_type,
                                  additional_brand_version);
}

// Return UserAgentBrandList with the major version populated in the brand
// `version` value.
// TODO(crbug.com/1291612): Consolidate *MajorVersionList() methods by using
// GetVersionNumber()
const blink::UserAgentBrandList GetUserAgentBrandMajorVersionListInternal(
    std::optional<blink::UserAgentBrandVersion> additional_brand_version) {
  return GetUserAgentBrandList(version_info::GetMajorVersionNumber(),
                               std::string(version_info::GetVersionNumber()),
                               blink::UserAgentBrandVersionType::kMajorVersion,
                               additional_brand_version);
}

// Return UserAgentBrandList with the full version populated in the brand
// `version` value.
// TODO(crbug.com/1291612): Consolidate *FullVersionList() methods by using
// GetVersionNumber()
const blink::UserAgentBrandList GetUserAgentBrandFullVersionListInternal(
    std::optional<blink::UserAgentBrandVersion> additional_brand_version) {
  return GetUserAgentBrandList(version_info::GetMajorVersionNumber(),
                               std::string(version_info::GetVersionNumber()),
                               blink::UserAgentBrandVersionType::kFullVersion,
                               additional_brand_version);
}

// Internal function to handle return the full or "reduced" user agent string,
// depending on the UserAgentReduction enterprise policy.
std::string GetUserAgentInternal(
    UserAgentReductionEnterprisePolicyState user_agent_reduction) {
  std::string product = GetProductAndVersion(user_agent_reduction);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kHeadless)) {
    product.insert(0, "Headless");
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kUseMobileUserAgent)) {
    product += " Mobile";
  }
#endif

  // In User-Agent reduction phase 5, only apply the <unifiedPlatform> to
  // desktop UA strings.
  // In User-Agent reduction phase 6, only apply the <unifiedPlatform> to
  // android UA strings.
  return ShouldSendUserAgentUnifiedPlatform(user_agent_reduction)
             ? BuildUnifiedPlatformUserAgentFromProduct(product)
             : BuildUserAgentFromProduct(product);
}

// Generate random order list based on the input size and seed.
// Manually implement a stable permutation shuffle since STL random number
// engines and generators are banned and helpers in base/rand_util.h not
// supported seed shuffle.
std::vector<size_t> GetRandomOrder(int seed, size_t size) {
  CHECK_GE(size, 2u);
  CHECK_LE(size, 4u);

  if (size == 2u) {
    return {seed % size, (seed + 1) % size};
  } else if (size == 3u) {
    // Pick a stable permutation seeded by major version number. any values here
    // and in order should be under three.
    static constexpr std::array<std::array<size_t, 3>, 6> orders{
        {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}}};
    const std::array<size_t, 3> order = orders[seed % orders.size()];
    return std::vector<size_t>(order.begin(), order.end());
  } else {
    // Pick a stable permutation seeded by major version number. any values
    // here and in order should be under four.
    static constexpr std::array<std::array<size_t, 4>, 24> orders{
        {{0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2},
         {0, 3, 2, 1}, {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0},
         {1, 3, 0, 2}, {1, 3, 2, 0}, {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3},
         {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0}, {3, 0, 1, 2}, {3, 0, 2, 1},
         {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}}};
    const std::array<size_t, 4> order = orders[seed % orders.size()];
    return std::vector<size_t>(order.begin(), order.end());
  }
}

// Shuffle the generated brand version list based on the seed.
blink::UserAgentBrandList ShuffleBrandList(
    blink::UserAgentBrandList brand_version_list,
    int seed) {
  const std::vector<size_t> order =
      GetRandomOrder(seed, brand_version_list.size());
  CHECK_EQ(brand_version_list.size(), order.size());

  blink::UserAgentBrandList shuffled_brand_version_list(
      brand_version_list.size());
  for (size_t i = 0; i < order.size(); i++) {
    shuffled_brand_version_list[order[i]] = brand_version_list[i];
  }

  return shuffled_brand_version_list;
}

std::string GetUserAgentPlatform() {
#if BUILDFLAG(IS_WIN)
  return "";
#elif BUILDFLAG(IS_MAC)
  return "Macintosh; ";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return "X11; ";  // strange, but that's what Firefox uses
#elif BUILDFLAG(IS_ANDROID)
  return "Linux; ";
#elif BUILDFLAG(IS_FUCHSIA)
  return "";
#elif BUILDFLAG(IS_IOS)
  return ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
             ? "iPad; "
             : "iPhone; ";
#else
#error Unsupported platform
#endif
}

std::string GetUnifiedPlatform() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
  // This constant is only used on Android (desktop) and Linux.
  constexpr char kUnifiedPlatformLinuxX64[] = "X11; Linux x86_64";
#endif
#if BUILDFLAG(IS_ANDROID)
  // The Android XR device by default also has the unified platform of desktop
  // form factor.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP ||
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_XR) {
    return kUnifiedPlatformLinuxX64;
  }
  return "Linux; Android 10; K";
#elif BUILDFLAG(IS_CHROMEOS)
  return "X11; CrOS x86_64 14541.0.0";
#elif BUILDFLAG(IS_MAC)
  return "Macintosh; Intel Mac OS X 10_15_7";
#elif BUILDFLAG(IS_WIN)
  return "Windows NT 10.0; Win64; x64";
#elif BUILDFLAG(IS_FUCHSIA)
  return "Fuchsia";
#elif BUILDFLAG(IS_LINUX)
  return kUnifiedPlatformLinuxX64;
#elif BUILDFLAG(IS_IOS)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return "iPad; CPU iPad OS 14_0 like Mac OS X";
  }
  return "iPhone; CPU iPhone OS 14_0 like Mac OS X";
#else
#error Unsupported platform
#endif
}

// Builds a string that describes the CPU type when available (or blank
// otherwise).
std::string BuildCpuInfo() {
  std::string cpuinfo;

#if BUILDFLAG(IS_MAC)
  cpuinfo = "Intel";
#elif BUILDFLAG(IS_IOS)
  cpuinfo = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
                ? "iPad"
                : "iPhone";
#elif BUILDFLAG(IS_WIN)
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  if (os_info->IsWowX86OnAMD64()) {
    cpuinfo = "WOW64";
  } else {
    base::win::OSInfo::WindowsArchitecture windows_architecture =
        os_info->GetArchitecture();
    if (windows_architecture == base::win::OSInfo::X64_ARCHITECTURE) {
      cpuinfo = "Win64; x64";
    } else if (windows_architecture == base::win::OSInfo::IA64_ARCHITECTURE) {
      cpuinfo = "Win64; IA64";
    }
  }
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  // special case for biarch systems
  if (UNSAFE_TODO(strcmp(unixinfo.machine, "x86_64")) == 0 &&
      sizeof(void*) == sizeof(int32_t)) {
    cpuinfo.assign("i686 (x86_64)");
  } else {
    cpuinfo.assign(unixinfo.machine);
  }
#endif

  return cpuinfo;
}

// Returns the OS version.
// On Android, the string will only include the build number and model if
// relevant enums indicate they should be included.
std::string GetOSVersion(IncludeAndroidBuildNumber include_android_build_number,
                         IncludeAndroidModel include_android_model) {
  std::string os_version;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS)
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);

#if BUILDFLAG(IS_MAC)
  // A significant amount of web content breaks if the reported "Mac
  // OS X" major version number is greater than 10. Continue to report
  // this as 10_15_7, the last dot release for that macOS version.
  if (os_major_version > 10) {
    os_major_version = 10;
    os_minor_version = 15;
    os_bugfix_version = 7;
  }
#endif

#endif

#if BUILDFLAG(IS_ANDROID)
  std::string android_version_str = base::SysInfo::OperatingSystemVersion();
  std::string android_info_str =
      GetAndroidOSInfo(include_android_build_number, include_android_model);
#endif

  base::StringAppendF(&os_version,
#if BUILDFLAG(IS_WIN)
                      "%d.%d", os_major_version, os_minor_version
#elif BUILDFLAG(IS_MAC)
                      "%d_%d_%d", os_major_version, os_minor_version,
                      os_bugfix_version
#elif BUILDFLAG(IS_IOS)
                      "%d_%d", os_major_version, os_minor_version
#elif BUILDFLAG(IS_CHROMEOS)
                      "%d.%d.%d", os_major_version, os_minor_version,
                      os_bugfix_version
#elif BUILDFLAG(IS_ANDROID)
                      "%s%s", android_version_str.c_str(),
                      android_info_str.c_str()
#else
                      ""
#endif
  );
  return os_version;
}

// Builds a User-agent compatible string that describes the OS and CPU type.
// On Android, the string will only include the build number and model if
// relevant enums indicate they should be included.
std::string BuildOSCpuInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model) {
  return BuildOSCpuInfoFromOSVersionAndCpuType(
      GetOSVersion(include_android_build_number, include_android_model),
      BuildCpuInfo());
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

const blink::UserAgentBrandList GetUserAgentBrandMajorVersionList(
    std::optional<blink::UserAgentBrandVersion> additional_brand_version) {
  return GetUserAgentBrandMajorVersionListInternal(additional_brand_version);
}

const blink::UserAgentBrandList GetUserAgentBrandFullVersionList(
    std::optional<blink::UserAgentBrandVersion> additional_brand_version) {
  return GetUserAgentBrandFullVersionListInternal(additional_brand_version);
}

// Generate a pseudo-random permutation of the following brand/version pairs:
//   1. The base project (i.e. Chromium)
//   2. The browser brand, if available
//   3. A randomized string containing GREASE characters to ensure proper
//      header parsing, along with an arbitrarily low version to ensure proper
//      version checking.
//   4. Additional brand/version pairs.
blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    std::optional<std::string> brand,
    const std::string& version,
    blink::UserAgentBrandVersionType output_version_type,
    std::optional<blink::UserAgentBrandVersion> additional_brand_version) {
  DCHECK_GE(seed, 0);

  blink::UserAgentBrandVersion greasey_bv =
      GetGreasedUserAgentBrandVersion(seed, output_version_type);
  blink::UserAgentBrandVersion chromium_bv = {"Chromium", version};

  blink::UserAgentBrandList brand_version_list = {std::move(greasey_bv),
                                                  std::move(chromium_bv)};
  if (brand) {
    brand_version_list.emplace_back(brand.value(), version);
  }
  if (additional_brand_version) {
    brand_version_list.emplace_back(additional_brand_version.value());
  }

  return ShuffleBrandList(brand_version_list, seed);
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
    int seed,
    blink::UserAgentBrandVersionType output_version_type) {
  std::string greasey_brand;
  std::string greasey_version;
  const std::vector<std::string> greasey_chars = {" ", "(", ":", "-", ".", "/",
                                                  ")", ";", "=", "?", "_"};
  const std::vector<std::string> greased_versions = {"8", "99", "24"};
  // See the spec:
  // https://wicg.github.io/ua-client-hints/#create-arbitrary-brands-section
  greasey_brand =
      base::StrCat({"Not", greasey_chars[(seed) % greasey_chars.size()], "A",
                    greasey_chars[(seed + 1) % greasey_chars.size()], "Brand"});
  greasey_version = greased_versions[seed % greased_versions.size()];
  return GetProcessedGreasedBrandVersion(greasey_brand, greasey_version,
                                         output_version_type);
}

bool GetMobileBitForUAMetadata() {
  // The mobile bit for UA-CH is true if the platform is iOS, or if it's
  // Android and not a desktop form factor, AND the kUseMobileUserAgent switch
  // is present.
#if BUILDFLAG(IS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP ||
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_XR) {
    return false;
  }
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kUseMobileUserAgent);
#else
  return false;
#endif
}

std::string GetPlatformVersion() {
#if BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/40245146): Remove this Blink feature
  if (base::FeatureList::IsEnabled(
          blink::features::kReduceUserAgentDataLinuxPlatformVersion)) {
    return std::string();
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP ||
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_XR) {
    return std::string();
  }
#endif

#if BUILDFLAG(IS_WIN)
  return GetWindowsPlatformVersion();
#elif BUILDFLAG(IS_FUCHSIA)
  return std::string();
#else

  int32_t major, minor, bugfix = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  return base::StringPrintf("%d.%d.%d", major, minor, bugfix);
#endif
}

std::string GetPlatformForUAMetadata() {
#if BUILDFLAG(IS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP ||
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_XR) {
    return "Linux";
  }
#endif

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40704421): This can be removed/re-refactored once we use
  // "macOS" by default
  return "macOS";
#elif BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40846294): The branding change to remove the space caused a
  // regression that's solved here. Ideally, we would just use the new OS name
  // without the space here too, but that needs a launch plan.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "Chrome OS";
#else
  return "Chromium OS";
#endif
#else
  return std::string(version_info::GetOSType());
#endif
}

blink::UserAgentMetadata GetUserAgentMetadata(bool only_low_entropy_ch) {
  blink::UserAgentMetadata metadata;

  // Low entropy client hints.
  metadata.brand_version_list =
      GetUserAgentBrandMajorVersionListInternal(std::nullopt);
  metadata.mobile = GetMobileBitForUAMetadata();
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
      GetUserAgentBrandFullVersionListInternal(std::nullopt);
  metadata.full_version = std::string(version_info::GetVersionNumber());
  metadata.architecture = GetCpuArchitecture();
  metadata.model = BuildModelInfo();
  metadata.form_factors = GetFormFactorsClientHint(metadata, metadata.mobile);
  metadata.bitness = GetCpuBitness();
  metadata.wow64 = IsWoW64();
  metadata.platform_version = GetPlatformVersion();
  return metadata;
}

std::vector<std::string> GetFormFactorsClientHint(
    const blink::UserAgentMetadata& metadata,
    bool is_mobile) {
  // By default, use "Mobile" or "Desktop" depending on the `mobile` bit.
  std::vector<std::string> form_factors = {
      is_mobile ? blink::kMobileFormFactor : blink::kDesktopFormFactor};

#if BUILDFLAG(IS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_XR) {
    form_factors.push_back(blink::kXRFormFactor);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return form_factors;
}

#if BUILDFLAG(IS_WIN)
int GetHighestKnownUniversalApiContractVersionForTesting() {
  return kHighestKnownUniversalApiContractVersion;
}
#endif  // BUILDFLAG(IS_WIN)

UserAgentReductionEnterprisePolicyState GetUserAgentReductionFromPrefs(
    const PrefService* pref_service) {
  if (!pref_service->HasPrefPath(kReduceUserAgentMinorVersion)) {
    return UserAgentReductionEnterprisePolicyState::kDefault;
  }
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

std::string GetUnifiedPlatformForTesting() {
  return GetUnifiedPlatform();
}

// Inaccurately named for historical reasons
std::string GetWebKitVersion() {
  return base::StringPrintf("537.36 (%s)", CHROMIUM_GIT_REVISION);
}

std::string GetChromiumGitRevision() {
  return CHROMIUM_GIT_REVISION;
}

// Return the CPU architecture in Windows/Mac/POSIX/Fuchsia and the empty string
// on Android or if unknown.
std::string GetCpuArchitecture() {
#if BUILDFLAG(IS_WIN)
  base::win::OSInfo::WindowsArchitecture windows_architecture =
      base::win::OSInfo::GetInstance()->GetArchitecture();
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  // When running a Chrome x86_64 (AMD64) build on an ARM64 device,
  // the OS lies and returns 0x9 (PROCESSOR_ARCHITECTURE_AMD64)
  // for wProcessorArchitecture.
  if (windows_architecture == base::win::OSInfo::ARM64_ARCHITECTURE ||
      os_info->IsWowX86OnARM64() || os_info->IsWowAMD64OnARM64()) {
    return "arm";
  } else if ((windows_architecture == base::win::OSInfo::X86_ARCHITECTURE) ||
             (windows_architecture == base::win::OSInfo::X64_ARCHITECTURE)) {
    return "x86";
  }
#elif BUILDFLAG(IS_MAC)
  base::mac::CPUType cpu_type = base::mac::GetCPUType();
  if (cpu_type == base::mac::CPUType::kIntel) {
    return "x86";
  } else if (cpu_type == base::mac::CPUType::kArm ||
             cpu_type == base::mac::CPUType::kTranslatedIntel) {
    return "arm";
  }
#elif BUILDFLAG(IS_IOS)
  return "arm";
#elif BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/433345971) The user agent string should contain the actual
  // cpu type information obtained from the Android device. Same for the cpu bit
  // count in #GetCpuBitness below.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP ||
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_XR) {
    return "x86";
  }
  return std::string();
#elif BUILDFLAG(IS_POSIX)
  std::string cpu_info = BuildCpuInfo();
  if (base::StartsWith(cpu_info, "arm") ||
      base::StartsWith(cpu_info, "aarch")) {
    return "arm";
  } else if ((base::StartsWith(cpu_info, "i") &&
              cpu_info.substr(2, 2) == "86") ||
             base::StartsWith(cpu_info, "x86")) {
    return "x86";
  }
#elif BUILDFLAG(IS_FUCHSIA)
  std::string cpu_arch = base::SysInfo::ProcessCPUArchitecture();
  if (base::StartsWith(cpu_arch, "x86")) {
    return "x86";
  } else if (base::StartsWith(cpu_arch, "ARM")) {
    return "arm";
  }
#else
#error Unsupported platform
#endif
  DLOG(WARNING) << "Unrecognized CPU Architecture";
  return std::string();
}

// Return the CPU bitness in Windows/Mac/POSIX/Fuchsia and the empty string
// on Android.
std::string GetCpuBitness() {
#if BUILDFLAG(IS_WIN)
  return (base::win::OSInfo::GetInstance()->GetArchitecture() ==
          base::win::OSInfo::X86_ARCHITECTURE)
             ? "32"
             : "64";
#elif BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FUCHSIA)
  return "64";
#elif BUILDFLAG(IS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP ||
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_XR) {
    return "64";
  }
  return std::string();
#elif BUILDFLAG(IS_POSIX)
  return base::Contains(BuildCpuInfo(), "64") ? "64" : "32";
#else
#error Unsupported platform
#endif
}

std::string BuildOSCpuInfoFromOSVersionAndCpuType(const std::string& os_version,
                                                  const std::string& cpu_type) {
  std::string os_cpu;

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);
#endif

#if BUILDFLAG(IS_WIN)
  if (!cpu_type.empty()) {
    base::StringAppendF(&os_cpu, "Windows NT %s; %s", os_version.c_str(),
                        cpu_type.c_str());
  } else {
    base::StringAppendF(&os_cpu, "Windows NT %s", os_version.c_str());
  }
#else
  base::StringAppendF(&os_cpu,
#if BUILDFLAG(IS_MAC)
                      "%s Mac OS X %s", cpu_type.c_str(), os_version.c_str()
#elif BUILDFLAG(IS_CHROMEOS)
                      "CrOS "
                      "%s %s",
                      cpu_type.c_str(),  // e.g. i686
                      os_version.c_str()
#elif BUILDFLAG(IS_ANDROID)
                      "Android %s", os_version.c_str()
#elif BUILDFLAG(IS_FUCHSIA)
                      "Fuchsia"
#elif BUILDFLAG(IS_IOS)
                      "CPU %s OS %s like Mac OS X", cpu_type.c_str(),
                      os_version.c_str()
#elif BUILDFLAG(IS_POSIX)
                      "%s %s",
                      unixinfo.sysname,  // e.g. Linux
                      cpu_type.c_str()   // e.g. i686
#endif
  );
#endif

  return os_cpu;
}

std::string BuildUnifiedPlatformUserAgentFromProduct(
    const std::string& product) {
  return BuildUserAgentFromOSAndProduct(GetUnifiedPlatform(), product);
}

std::string BuildUserAgentFromProduct(const std::string& product) {
  std::string os_info;
  base::StringAppendF(&os_info, "%s%s", GetUserAgentPlatform().c_str(),
                      BuildOSCpuInfo(IncludeAndroidBuildNumber::Exclude,
                                     IncludeAndroidModel::Include)
                          .c_str());
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string BuildModelInfo() {
#if BUILDFLAG(IS_ANDROID)
  // Model information is not exposed on Android desktop.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_DESKTOP) {
    return std::string();
  }

  // Only send the model information if on the release build of Android,
  // matching user agent behaviour.
  if (base::SysInfo::GetAndroidBuildCodename() == "REL") {
    return base::SysInfo::HardwareModelName();
  }
#endif

  return std::string();
}

#if BUILDFLAG(IS_ANDROID)
std::string BuildUserAgentFromProductAndExtraOSInfo(
    const std::string& product,
    const std::string& extra_os_info,
    IncludeAndroidBuildNumber include_android_build_number) {
  std::string os_info;
  base::StrAppend(&os_info, {GetUserAgentPlatform(),
                             BuildOSCpuInfo(include_android_build_number,
                                            IncludeAndroidModel::Include),
                             extra_os_info});
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string BuildUnifiedPlatformUAFromProductAndExtraOs(
    const std::string& product,
    const std::string& extra_os_info) {
  std::string os_info;
  base::StrAppend(&os_info, {GetUnifiedPlatform(), extra_os_info});
  return BuildUserAgentFromOSAndProduct(os_info, product);
}

std::string GetAndroidOSInfo(
    IncludeAndroidBuildNumber include_android_build_number,
    IncludeAndroidModel include_android_model) {
  std::string android_info_str;

  // Send information about the device.
  bool semicolon_inserted = false;
  if (include_android_model == IncludeAndroidModel::Include) {
    std::string android_device_name = BuildModelInfo();
    if (!android_device_name.empty()) {
      android_info_str += "; " + android_device_name;
      semicolon_inserted = true;
    }
  }

  // Append the build ID.
  if (include_android_build_number == IncludeAndroidBuildNumber::Include) {
    std::string android_build_id = base::SysInfo::GetAndroidBuildID();
    if (!android_build_id.empty()) {
      if (!semicolon_inserted) {
        android_info_str += ";";
      }
      android_info_str += " Build/" + android_build_id;
    }
  }

  return android_info_str;
}
#endif  // BUILDFLAG(IS_ANDROID)

std::string BuildUserAgentFromOSAndProduct(const std::string& os_info,
                                           const std::string& product) {
  // Derived from Safari's UA string.
  // This is done to expose our product name in a manner that is maximally
  // compatible with Safari, we hope!!
  std::string user_agent;
  base::StringAppendF(&user_agent,
                      "Mozilla/5.0 (%s) AppleWebKit/537.36 (KHTML, like Gecko) "
                      "%s Safari/537.36",
                      os_info.c_str(), product.c_str());
  return user_agent;
}

bool IsWoW64() {
#if BUILDFLAG(IS_WIN)
  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  return os_info->IsWowX86OnAMD64();
#else
  return false;
#endif
}

}  // namespace embedder_support
