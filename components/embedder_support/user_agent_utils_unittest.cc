// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/user_agent_utils.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#if defined(USE_X11) || defined(USE_OZONE)
#include <sys/utsname.h>
#endif

namespace embedder_support {

namespace {

void CheckUserAgentStringOrdering(bool mobile_device) {
  std::vector<std::string> pieces;

  // Check if the pieces of the user agent string come in the correct order.
  std::string buffer = GetUserAgent();

  pieces = base::SplitStringUsingSubstr(
      buffer, "Mozilla/5.0 (", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  EXPECT_EQ("", pieces[0]);

  pieces = base::SplitStringUsingSubstr(
      buffer, ") AppleWebKit/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string os_str = pieces[0];

  pieces =
      base::SplitStringUsingSubstr(buffer, " (KHTML, like Gecko) ",
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string webkit_version_str = pieces[0];

  pieces = base::SplitStringUsingSubstr(
      buffer, " Safari/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  std::string product_str = pieces[0];
  std::string safari_version_str = pieces[1];

  EXPECT_FALSE(os_str.empty());

  pieces = base::SplitStringUsingSubstr(os_str, "; ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
#if defined(OS_WIN)
  // Windows NT 10.0; Win64; x64
  // Windows NT 10.0; WOW64
  // Windows NT 10.0
  std::string os_and_version = pieces[0];
  for (unsigned int i = 1; i < pieces.size(); ++i) {
    bool equals = ((pieces[i] == "WOW64") || (pieces[i] == "Win64") ||
                   pieces[i] == "x64");
    ASSERT_TRUE(equals);
  }
  pieces = base::SplitStringUsingSubstr(pieces[0], " ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  ASSERT_EQ(3u, pieces.size());
  ASSERT_EQ("Windows", pieces[0]);
  ASSERT_EQ("NT", pieces[1]);
  double version;
  ASSERT_TRUE(base::StringToDouble(pieces[2], &version));
  ASSERT_LE(4.0, version);
  ASSERT_GT(11.0, version);
#elif defined(OS_MAC)
  // Macintosh; Intel Mac OS X 10_15_4
  ASSERT_EQ(2u, pieces.size());
  ASSERT_EQ("Macintosh", pieces[0]);
  pieces = base::SplitStringUsingSubstr(pieces[1], " ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  ASSERT_EQ(5u, pieces.size());
  ASSERT_EQ("Intel", pieces[0]);
  ASSERT_EQ("Mac", pieces[1]);
  ASSERT_EQ("OS", pieces[2]);
  ASSERT_EQ("X", pieces[3]);
  pieces = base::SplitStringUsingSubstr(pieces[4], "_", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  {
    int major, minor, patch;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &patch);
    // crbug.com/1175225
    if (major > 10)
      major = 10;
    ASSERT_EQ(base::StringPrintf("%d", major), pieces[0]);
  }
  int value;
  ASSERT_TRUE(base::StringToInt(pieces[1], &value));
  ASSERT_LE(0, value);
  ASSERT_TRUE(base::StringToInt(pieces[2], &value));
  ASSERT_LE(0, value);
#elif defined(USE_X11) || defined(USE_OZONE)
  // X11; Linux x86_64
  // X11; CrOS armv7l 4537.56.0
  struct utsname unixinfo;
  uname(&unixinfo);
  std::string machine = unixinfo.machine;
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32_t)) {
    machine = "i686 (x86_64)";
  }
  ASSERT_EQ(2u, pieces.size());
  ASSERT_EQ("X11", pieces[0]);
  pieces = base::SplitStringUsingSubstr(pieces[1], " ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // X11; CrOS armv7l 4537.56.0
  //      ^^
  ASSERT_EQ(3u, pieces.size());
  ASSERT_EQ("CrOS", pieces[0]);
  ASSERT_EQ(machine, pieces[1]);
  pieces = base::SplitStringUsingSubstr(pieces[2], ".", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  for (unsigned int i = 1; i < pieces.size(); ++i) {
    int value;
    ASSERT_TRUE(base::StringToInt(pieces[i], &value));
  }
#else
  // X11; Linux x86_64
  //      ^^
  ASSERT_EQ(2u, pieces.size());
  // This may not be Linux in all cases in the wild, but it is on the bots.
  ASSERT_EQ("Linux", pieces[0]);
  ASSERT_EQ(machine, pieces[1]);
#endif
#elif defined(OS_ANDROID)
  // Linux; Android 7.1.1; Samsung Chromebook 3
  ASSERT_GE(3u, pieces.size());
  ASSERT_EQ("Linux", pieces[0]);
  std::string model;
  if (pieces.size() > 2)
    model = pieces[2];

  pieces = base::SplitStringUsingSubstr(pieces[1], " ", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  ASSERT_EQ("Android", pieces[0]);
  pieces = base::SplitStringUsingSubstr(pieces[1], ".", base::KEEP_WHITESPACE,
                                        base::SPLIT_WANT_ALL);
  for (unsigned int i = 1; i < pieces.size(); ++i) {
    int value;
    ASSERT_TRUE(base::StringToInt(pieces[i], &value));
  }

  if (!model.empty()) {
    if (base::SysInfo::GetAndroidBuildCodename() == "REL")
      ASSERT_EQ(base::SysInfo::HardwareModelName(), model);
    else
      ASSERT_EQ("", model);
  }
#elif defined(OS_FUCHSIA)
  // X11; Fuchsia
  ASSERT_EQ(2u, pieces.size());
  ASSERT_EQ("X11", pieces[0]);
  ASSERT_EQ("Fuchsia", pieces[1]);
#endif

  // Check that the version numbers match.
  EXPECT_FALSE(webkit_version_str.empty());
  EXPECT_FALSE(safari_version_str.empty());
  EXPECT_EQ(webkit_version_str, safari_version_str);

  EXPECT_TRUE(
      base::StartsWith(product_str, "Chrome/", base::CompareCase::SENSITIVE));
  if (mobile_device) {
    // "Mobile" gets tacked on to the end for mobile devices, like phones.
    EXPECT_TRUE(
        base::EndsWith(product_str, " Mobile", base::CompareCase::SENSITIVE));
  }
}

}  // namespace

TEST(UserAgentUtilsTest, UserAgentStringOrdering) {
#if defined(OS_ANDROID)
  const char* const kArguments[] = {"chrome"};
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->InitFromArgv(1, kArguments);

  // Do it for regular devices.
  ASSERT_FALSE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  CheckUserAgentStringOrdering(false);

  // Do it for mobile devices.
  command_line->AppendSwitch(switches::kUseMobileUserAgent);
  ASSERT_TRUE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  CheckUserAgentStringOrdering(true);
#else
  CheckUserAgentStringOrdering(false);
#endif
}

TEST(UserAgentUtilsTest, UserAgentStringFrozen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kReduceUserAgent);

#if defined(OS_ANDROID)
  // Verify the correct user agent is returned when the UseMobileUserAgent
  // command line flag is present.
  const char* const kArguments[] = {"chrome"};
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->InitFromArgv(1, kArguments);

  // Verify the mobile user agent string is not returned when not using a mobile
  // user agent.
  ASSERT_FALSE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  {
    std::string buffer = GetUserAgent();
    std::string device_compat = "";
    EXPECT_EQ(buffer,
              base::StringPrintf(content::frozen_user_agent_strings::kAndroid,
                                 content::GetUnifiedPlatform().c_str(),
                                 version_info::GetMajorVersionNumber().c_str(),
                                 device_compat.c_str()));
  }

  // Verify the mobile user agent string is returned when using a mobile user
  // agent.
  command_line->AppendSwitch(switches::kUseMobileUserAgent);
  ASSERT_TRUE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  {
    std::string buffer = GetUserAgent();
    std::string device_compat = "Mobile ";
    EXPECT_EQ(buffer,
              base::StringPrintf(content::frozen_user_agent_strings::kAndroid,
                                 content::GetUnifiedPlatform().c_str(),
                                 version_info::GetMajorVersionNumber().c_str(),
                                 device_compat.c_str()));
  }
#else
  {
    std::string buffer = GetUserAgent();
    EXPECT_EQ(buffer, base::StringPrintf(
                          content::frozen_user_agent_strings::kDesktop,
                          content::GetUnifiedPlatform().c_str(),
                          version_info::GetMajorVersionNumber().c_str()));
  }
#endif
}

TEST(UserAgentUtilsTest, UserAgentMetadata) {
  auto metadata = GetUserAgentMetadata();

  std::string major_version = version_info::GetMajorVersionNumber();

  // According to spec, Sec-CH-UA should contain what project the browser is
  // based on (i.e. Chromium in this case) as well as the actual product.
  // In CHROMIUM_BRANDING builds this will check chromium twice. That should be
  // ok though.

  const blink::UserAgentBrandVersion chromium_brand_version = {"Chromium",
                                                               major_version};
  const blink::UserAgentBrandVersion product_brand_version = {
      version_info::GetProductName(), version_info::GetMajorVersionNumber()};
  bool contains_chromium_brand_version = false;
  bool contains_product_brand_version = false;

  for (const auto& brand_version : metadata.brand_version_list) {
    if (brand_version == chromium_brand_version) {
      contains_chromium_brand_version = true;
    }
    if (brand_version == product_brand_version) {
      contains_product_brand_version = true;
    }
  }

  EXPECT_TRUE(contains_chromium_brand_version);
  EXPECT_TRUE(contains_product_brand_version);

  EXPECT_EQ(metadata.full_version, version_info::GetVersionNumber());

  int32_t major, minor, bugfix = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  EXPECT_EQ(metadata.platform_version,
            base::StringPrintf("%d.%d.%d", major, minor, bugfix));
  // This makes sure no extra information is added to the platform version.
  EXPECT_EQ(metadata.platform_version.find(";"), std::string::npos);
  // TODO(crbug.com/1103047): This can be removed/re-refactored once we use
  // "macOS" by default
#if defined(OS_MAC)
  EXPECT_EQ(metadata.platform, "macOS");
#else
  EXPECT_EQ(metadata.platform, version_info::GetOSType());
#endif
  EXPECT_EQ(metadata.architecture, content::GetLowEntropyCpuArchitecture());
  EXPECT_EQ(metadata.model, content::BuildModelInfo());
  EXPECT_EQ(metadata.bitness, content::GetLowEntropyCpuBitness());
}

TEST(UserAgentUtilsTest, GenerateBrandVersionList) {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list =
      GenerateBrandVersionList(84, absl::nullopt, "84", absl::nullopt);
  std::string brand_list = metadata.SerializeBrandVersionList();
  EXPECT_EQ(R"(" Not A;Brand";v="99", "Chromium";v="84")", brand_list);

  metadata.brand_version_list =
      GenerateBrandVersionList(85, absl::nullopt, "85", absl::nullopt);
  std::string brand_list_diff = metadata.SerializeBrandVersionList();
  // Make sure the lists are different for different seeds
  EXPECT_EQ(R"("Chromium";v="85", " Not;A Brand";v="99")", brand_list_diff);
  EXPECT_NE(brand_list, brand_list_diff);

  metadata.brand_version_list =
      GenerateBrandVersionList(84, "Totally A Brand", "84", absl::nullopt);
  std::string brand_list_w_brand = metadata.SerializeBrandVersionList();
  EXPECT_EQ(
      R"(" Not A;Brand";v="99", "Chromium";v="84", "Totally A Brand";v="84")",
      brand_list_w_brand);

  metadata.brand_version_list =
      GenerateBrandVersionList(84, absl::nullopt, "84", "Clean GREASE");
  std::string brand_list_grease_override = metadata.SerializeBrandVersionList();
  EXPECT_EQ(R"("Clean GREASE";v="99", "Chromium";v="84")",
            brand_list_grease_override);
  EXPECT_NE(brand_list, brand_list_grease_override);

  // Should DCHECK on negative numbers
  EXPECT_DCHECK_DEATH(
      GenerateBrandVersionList(-1, absl::nullopt, "99", absl::nullopt));
}

}  // namespace embedder_support
