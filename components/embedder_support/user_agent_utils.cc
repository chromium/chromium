// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/user_agent_utils.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "components/embedder_support/switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace embedder_support {

std::string GetProduct() {
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

  if (base::FeatureList::IsEnabled(blink::features::kReduceUserAgent)) {
    return content::GetReducedUserAgent(
        command_line->HasSwitch(switches::kUseMobileUserAgent),
        version_info::GetMajorVersionNumber());
  }

  std::string product = GetProduct();
#if defined(OS_ANDROID)
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return content::BuildUserAgentFromProduct(product);
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

const blink::UserAgentBrandList& GetBrandVersionList() {
  static const base::NoDestructor<blink::UserAgentBrandList>
      greased_brand_version_list([] {
        int major_version_number;
        std::string major_version = version_info::GetMajorVersionNumber();
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

        return GenerateBrandVersionList(major_version_number, brand,
                                        major_version, maybe_param_override);
      }());
  return *greased_brand_version_list;
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
  metadata.full_version = version_info::GetVersionNumber();
  metadata.platform = GetPlatformForUAMetadata();
  metadata.architecture = content::GetLowEntropyCpuArchitecture();
  metadata.model = content::BuildModelInfo();
  metadata.mobile = false;
#if defined(OS_ANDROID)
  metadata.mobile = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseMobileUserAgent);
#endif

  int32_t major, minor, bugfix = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  metadata.platform_version =
      base::StringPrintf("%d.%d.%d", major, minor, bugfix);
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
  std::string product = version_info::GetProductNameAndVersionForUserAgent();

  blink::UserAgentOverride spoofed_ua;
  spoofed_ua.ua_string_override =
      content::BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
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

}  // namespace embedder_support
