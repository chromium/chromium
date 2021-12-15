// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/user_agent_utils.h"

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_brand_version_type.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/re2/src/re2/re2.h"

#if defined(USE_OZONE)
#include <sys/utsname.h>
#endif

#if defined(OS_WIN)
#include <windows.foundation.metadata.h>
#include <wrl.h>

#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#endif  // defined(OS_WIN)

namespace embedder_support {

namespace {

// A regular expression that matches Chrome/{major_version}.{minor_version} in
// the User-Agent string, where the first capture is the {major_version} and the
// second capture is the {minor_version}.
static constexpr char kChromeProductVersionRegex[] =
    "Chrome/([0-9]+)\\.([0-9]+\\.[0-9]+\\.[0-9]+)";

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
#elif defined(USE_OZONE)
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
#if defined(OS_CHROMEOS)
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

#if defined(OS_WIN)
bool ResolveCoreWinRT() {
  return base::win::ResolveCoreWinRTDelayload() &&
         base::win::ScopedHString::ResolveCoreWinRTStringDelayload() &&
         base::win::HStringReference::ResolveCoreWinRTStringDelayload();
}

// On Windows, the client hint sec-ch-ua-platform-version should be
// the highest supported version of the UniversalApiContract.
void VerifyWinPlatformVersion(std::string version) {
  ASSERT_TRUE(ResolveCoreWinRT());
  base::win::ScopedWinrtInitializer scoped_winrt_initializer;
  ASSERT_TRUE(scoped_winrt_initializer.Succeeded());

  base::win::HStringReference api_info_class_name(
      RuntimeClass_Windows_Foundation_Metadata_ApiInformation);

  Microsoft::WRL::ComPtr<
      ABI::Windows::Foundation::Metadata::IApiInformationStatics>
      api;
  HRESULT result = base::win::RoGetActivationFactory(api_info_class_name.Get(),
                                                     IID_PPV_ARGS(&api));
  ASSERT_EQ(result, S_OK);

  base::win::HStringReference universal_contract_name(
      L"Windows.Foundation.UniversalApiContract");

  std::vector<std::string> version_parts = base::SplitString(
      version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  EXPECT_EQ(version_parts[2], "0");

  int major_version;
  base::StringToInt(version_parts[0], &major_version);

  // If this check fails, our highest known UniversalApiContract version
  // needs to be updated.
  EXPECT_LE(major_version,
            GetHighestKnownUniversalApiContractVersionForTesting());

  int minor_version;
  base::StringToInt(version_parts[1], &minor_version);

  boolean is_supported = false;
  // Verify that the major and minor versions are supported.
  result = api->IsApiContractPresentByMajor(universal_contract_name.Get(),
                                            major_version, &is_supported);
  EXPECT_EQ(result, S_OK);
  EXPECT_TRUE(is_supported)
      << " expected major version " << major_version << " to be supported.";
  result = api->IsApiContractPresentByMajorAndMinor(
      universal_contract_name.Get(), major_version, minor_version,
      &is_supported);
  EXPECT_EQ(result, S_OK);
  EXPECT_TRUE(is_supported)
      << " expected major version " << major_version << " and minor version "
      << minor_version << " to be supported.";

  // Verify that the next highest value is not supported.
  result = api->IsApiContractPresentByMajorAndMinor(
      universal_contract_name.Get(), major_version, minor_version + 1,
      &is_supported);
  EXPECT_EQ(result, S_OK);
  EXPECT_FALSE(is_supported) << " expected minor version " << minor_version + 1
                             << " to not be supported with a major version of "
                             << major_version << ".";
  result = api->IsApiContractPresentByMajor(universal_contract_name.Get(),
                                            major_version + 1, &is_supported);
  EXPECT_EQ(result, S_OK);
  EXPECT_FALSE(is_supported) << " expected major version " << major_version + 1
                             << " to not be supported.";
}
#endif  // defined(OS_WIN)

}  // namespace

class UserAgentUtilsTest : public testing::Test,
                           public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (ForceMajorVersionTo100())
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kForceMajorVersion100InUserAgent);
  }

  bool ForceMajorVersionTo100() { return GetParam(); }

  std::string M100VersionNumber() {
    const base::Version version = version_info::GetVersion();
    std::string m100_version("100");
    // The rest of the version after the major version string is the same.
    for (size_t i = 1; i < version.components().size(); ++i) {
      m100_version.append(".");
      m100_version.append(base::NumberToString(version.components()[i]));
    }
    return m100_version;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_CASE_P(All,
                        UserAgentUtilsTest,
                        /*force_major_version_to_M100*/ testing::Bool());

TEST_P(UserAgentUtilsTest, UserAgentStringOrdering) {
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

TEST_P(UserAgentUtilsTest, UserAgentStringReduced) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(blink::features::kReduceUserAgent);

#if defined(OS_ANDROID)
  // Verify the correct user agent is returned when the UseMobileUserAgent
  // command line flag is present.
  const char* const kArguments[] = {"chrome"};
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->InitFromArgv(1, kArguments);
  const std::string major_version_number =
      version_info::GetMajorVersionNumber();
  const char* const major_version =
      ForceMajorVersionTo100() ? "100" : major_version_number.c_str();

  // Verify the mobile user agent string is not returned when not using a mobile
  // user agent.
  ASSERT_FALSE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  {
    std::string buffer = GetUserAgent();
    std::string device_compat = "";
    EXPECT_EQ(buffer,
              base::StringPrintf(content::frozen_user_agent_strings::kAndroid,
                                 content::GetUnifiedPlatform().c_str(),
                                 major_version, device_compat.c_str()));
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
                                 major_version, device_compat.c_str()));
  }
#else
  {
    std::string buffer = GetUserAgent();
    EXPECT_EQ(buffer, base::StringPrintf(
                          content::frozen_user_agent_strings::kDesktop,
                          content::GetUnifiedPlatform().c_str(),
                          ForceMajorVersionTo100()
                              ? "100"
                              : version_info::GetMajorVersionNumber().c_str()));
  }
#endif

  EXPECT_EQ(GetUserAgent(), GetReducedUserAgent());
}

TEST_P(UserAgentUtilsTest, UserAgentMetadata) {
  auto metadata = GetUserAgentMetadata();

  const std::string major_version =
      ForceMajorVersionTo100() ? "100" : version_info::GetMajorVersionNumber();

  const std::string full_version = ForceMajorVersionTo100()
                                       ? M100VersionNumber()
                                       : version_info::GetVersionNumber();

  // According to spec, Sec-CH-UA should contain what project the browser is
  // based on (i.e. Chromium in this case) as well as the actual product.
  // In CHROMIUM_BRANDING builds this will check chromium twice. That should be
  // ok though.

  const blink::UserAgentBrandVersion chromium_brand_version = {"Chromium",
                                                               major_version};
  const blink::UserAgentBrandVersion product_brand_version = {
      version_info::GetProductName(), major_version};
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

  // verify full version list
  const blink::UserAgentBrandVersion chromium_brand_full_version = {
      "Chromium", full_version};
  const blink::UserAgentBrandVersion product_brand_full_version = {
      version_info::GetProductName(), full_version};
  bool contains_chromium_brand_full_version = false;
  bool contains_product_brand_full_version = false;

  for (const auto& brand_version : metadata.brand_full_version_list) {
    if (brand_version == chromium_brand_full_version) {
      contains_chromium_brand_full_version = true;
    }
    if (brand_version == product_brand_full_version) {
      contains_product_brand_full_version = true;
    }
  }

  EXPECT_TRUE(contains_chromium_brand_full_version);
  EXPECT_TRUE(contains_product_brand_full_version);

  EXPECT_EQ(metadata.full_version, full_version);

#if defined(OS_WIN)
  if (base::win::GetVersion() < base::win::Version::WIN10) {
    EXPECT_EQ(metadata.platform_version, "0.0.0");
  } else {
    VerifyWinPlatformVersion(metadata.platform_version);
  }
#else
  int32_t major, minor, bugfix = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  EXPECT_EQ(metadata.platform_version,
            base::StringPrintf("%d.%d.%d", major, minor, bugfix));
#endif
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

TEST_P(UserAgentUtilsTest, GenerateBrandVersionList) {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list = GenerateBrandVersionList(
      84, absl::nullopt, "84", absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list = GenerateBrandVersionList(
      84, absl::nullopt, "84.0.0.0", absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  // 1. verify major version
  std::string brand_list = metadata.SerializeBrandMajorVersionList();
  EXPECT_EQ(R"(" Not A;Brand";v="99", "Chromium";v="84")", brand_list);
  // 2. verify full version
  std::string brand_list_w_fv = metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(R"(" Not A;Brand";v="99.0.0.0", "Chromium";v="84.0.0.0")",
            brand_list_w_fv);

  metadata.brand_version_list = GenerateBrandVersionList(
      85, absl::nullopt, "85", absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list = GenerateBrandVersionList(
      85, absl::nullopt, "85.0.0.0", absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  std::string brand_list_diff = metadata.SerializeBrandMajorVersionList();
  // Make sure the lists are different for different seeds
  // 1. verify major version
  EXPECT_EQ(R"("Chromium";v="85", " Not;A Brand";v="99")", brand_list_diff);
  EXPECT_NE(brand_list, brand_list_diff);
  // 2.verify full version
  std::string brand_list_diff_w_fv = metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(R"("Chromium";v="85.0.0.0", " Not;A Brand";v="99.0.0.0")",
            brand_list_diff_w_fv);
  EXPECT_NE(brand_list_w_fv, brand_list_diff_w_fv);

  metadata.brand_version_list = GenerateBrandVersionList(
      84, "Totally A Brand", "84", absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list = GenerateBrandVersionList(
      84, "Totally A Brand", "84.0.0.0", absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  // 1. verify major version
  std::string brand_list_w_brand = metadata.SerializeBrandMajorVersionList();
  EXPECT_EQ(
      R"(" Not A;Brand";v="99", "Chromium";v="84", "Totally A Brand";v="84")",
      brand_list_w_brand);
  // 2. verify full version
  std::string brand_list_w_brand_fv = metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(base::StrCat({"\" Not A;Brand\";v=\"99.0.0.0\", ",
                          "\"Chromium\";v=\"84.0.0.0\", ",
                          "\"Totally A Brand\";v=\"84.0.0.0\""}),
            brand_list_w_brand_fv);
  metadata.brand_version_list = GenerateBrandVersionList(
      84, absl::nullopt, "84", "Clean GREASE", absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list = GenerateBrandVersionList(
      84, absl::nullopt, "84.0.0.0", "Clean GREASE", absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  // 1. verify major version
  std::string brand_list_grease_override =
      metadata.SerializeBrandMajorVersionList();
  EXPECT_EQ(R"("Clean GREASE";v="99", "Chromium";v="84")",
            brand_list_grease_override);
  EXPECT_NE(brand_list, brand_list_grease_override);
  // 2. verify full version
  std::string brand_list_grease_override_fv =
      metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(R"("Clean GREASE";v="99.0.0.0", "Chromium";v="84.0.0.0")",
            brand_list_grease_override_fv);
  EXPECT_NE(brand_list_w_fv, brand_list_grease_override_fv);

  metadata.brand_version_list = GenerateBrandVersionList(
      84, absl::nullopt, "84", "Clean GREASE", "1024", true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list = GenerateBrandVersionList(
      84, absl::nullopt, "84.0.0.0", "Clean GREASE", "1024", true,
      blink::UserAgentBrandVersionType::kFullVersion);
  // 1. verify major version
  std::string brand_list_and_version_grease_override =
      metadata.SerializeBrandMajorVersionList();
  EXPECT_EQ(R"("Clean GREASE";v="1024", "Chromium";v="84")",
            brand_list_and_version_grease_override);
  EXPECT_NE(brand_list, brand_list_and_version_grease_override);
  // 2. verify full version
  std::string brand_list_and_version_grease_override_fv =
      metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(R"("Clean GREASE";v="1024.0.0.0", "Chromium";v="84.0.0.0")",
            brand_list_and_version_grease_override_fv);
  EXPECT_NE(brand_list_w_fv, brand_list_and_version_grease_override_fv);

  metadata.brand_version_list = GenerateBrandVersionList(
      84, absl::nullopt, "84", absl::nullopt, "1024", true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list = GenerateBrandVersionList(
      84, absl::nullopt, "84.0.0.0", absl::nullopt, "1024", true,
      blink::UserAgentBrandVersionType::kFullVersion);
  // 1. verify major version
  std::string brand_version_grease_override =
      metadata.SerializeBrandMajorVersionList();
  EXPECT_EQ(R"(" Not A;Brand";v="1024", "Chromium";v="84")",
            brand_version_grease_override);
  EXPECT_NE(brand_list, brand_version_grease_override);
  // 2. verify full version
  std::string brand_version_grease_override_fv =
      metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(R"(" Not A;Brand";v="1024.0.0.0", "Chromium";v="84.0.0.0")",
            brand_version_grease_override_fv);
  EXPECT_NE(brand_list_w_fv, brand_version_grease_override_fv);

  // Should DCHECK on negative numbers
  EXPECT_DCHECK_DEATH(GenerateBrandVersionList(
      -1, absl::nullopt, "99", absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion));
  EXPECT_DCHECK_DEATH(GenerateBrandVersionList(
      -1, absl::nullopt, "99.0.0.0", absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion));
}

TEST_P(UserAgentUtilsTest, GetGreasedUserAgentBrandVersion) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Test to ensure the old algorithm is respected when the flag is not set.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGreaseUACH, {{"updated_algorithm", "false"}});

  std::vector<int> permuted_order{0, 1, 2};
  blink::UserAgentBrandVersion greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, " Not A;Brand");
  EXPECT_EQ(greased_bv.version, "99");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, " Not A;Brand");
  EXPECT_EQ(greased_bv.version, "99.0.0.0");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, "WhatIsGrease", absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "WhatIsGrease");
  EXPECT_EQ(greased_bv.version, "99");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, "WhatIsGrease", absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "WhatIsGrease");
  EXPECT_EQ(greased_bv.version, "99.0.0.0");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, "1024", true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, " Not A;Brand");
  EXPECT_EQ(greased_bv.version, "1024");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, "1024", true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, " Not A;Brand");
  EXPECT_EQ(greased_bv.version, "1024.0.0.0");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, "WhatIsGrease", "1024", true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "WhatIsGrease");
  EXPECT_EQ(greased_bv.version, "1024");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, "WhatIsGrease", "1024", true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "WhatIsGrease");
  EXPECT_EQ(greased_bv.version, "1024.0.0.0");

  // Test to ensure the new algorithm works and is still overridable.
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGreaseUACH, {{"updated_algorithm", "true"}});

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "/Not=A?Brand");
  EXPECT_EQ(greased_bv.version, "8");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "/Not=A?Brand");
  EXPECT_EQ(greased_bv.version, "8.0.0.0");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, "WhatIsGrease", absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "WhatIsGrease");
  EXPECT_EQ(greased_bv.version, "8");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, "WhatIsGrease", absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "WhatIsGrease");
  EXPECT_EQ(greased_bv.version, "8.0.0.0");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, "1024", true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "/Not=A?Brand");
  EXPECT_EQ(greased_bv.version, "1024");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, "1024", true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "/Not=A?Brand");
  EXPECT_EQ(greased_bv.version, "1024.0.0.0");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, "WhatIsGrease", "1024", true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "WhatIsGrease");
  EXPECT_EQ(greased_bv.version, "1024");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, "WhatIsGrease", "1024", true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "WhatIsGrease");
  EXPECT_EQ(greased_bv.version, "1024.0.0.0");

  permuted_order = {2, 1, 0};
  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 86, absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, ";Not_A Brand");
  EXPECT_EQ(greased_bv.version, "24");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 86, absl::nullopt, absl::nullopt, true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, ";Not_A Brand");
  EXPECT_EQ(greased_bv.version, "24.0.0.0");

  // Test the greasy input with full version
  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, "1024.0.0.0", true,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "/Not=A?Brand");
  EXPECT_EQ(greased_bv.version, "1024");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, "1024.0.0.0", true,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "/Not=A?Brand");
  EXPECT_EQ(greased_bv.version, "1024.0.0.0");

  // Ensure the enterprise override bool takes precedence over the command line
  // flag
  permuted_order = {0, 1, 2};
  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, absl::nullopt, false,
      blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, " Not A;Brand");
  EXPECT_EQ(greased_bv.version, "99");

  greased_bv = GetGreasedUserAgentBrandVersion(
      permuted_order, 84, absl::nullopt, absl::nullopt, false,
      blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, " Not A;Brand");
  EXPECT_EQ(greased_bv.version, "99.0.0.0");

  // Go up to 110 based on the 11 total chars * 10 possible first chars.
  for (int i = 0; i < 110; i++) {
    // Regardless of the major version seed, the spec calls for no leading
    // whitespace in the brand.
    greased_bv = GetGreasedUserAgentBrandVersion(
        permuted_order, i, absl::nullopt, absl::nullopt, true,
        blink::UserAgentBrandVersionType::kMajorVersion);
    EXPECT_NE(greased_bv.brand[0], ' ');

    greased_bv = GetGreasedUserAgentBrandVersion(
        permuted_order, i, absl::nullopt, absl::nullopt, true,
        blink::UserAgentBrandVersionType::kFullVersion);
    EXPECT_NE(greased_bv.brand[0], ' ');
  }
}

TEST_P(UserAgentUtilsTest, GetProduct) {
  const std::string product = GetProduct();
  std::string major_version;
  EXPECT_TRUE(
      re2::RE2::FullMatch(product, kChromeProductVersionRegex, &major_version));
  // Whether the force M100 experiment is on or not, the product value should
  // contain the actual major version number.
  EXPECT_EQ(major_version, version_info::GetMajorVersionNumber());
}

TEST_P(UserAgentUtilsTest, GetUserAgent) {
  const std::string ua = GetUserAgent();
  std::string major_version;
  std::string minor_version;
  EXPECT_TRUE(re2::RE2::PartialMatch(ua, kChromeProductVersionRegex,
                                     &major_version, &minor_version));
  if (ForceMajorVersionTo100())
    EXPECT_EQ(major_version, "100");
  else
    EXPECT_EQ(major_version, version_info::GetMajorVersionNumber());
  EXPECT_NE(minor_version, "0.0.0");
}

class UserAgentUtilsMinorVersionTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (ForceMinorVersionTo100())
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kForceMinorVersion100InUserAgent);
  }

  bool ForceMinorVersionTo100() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_CASE_P(All,
                        UserAgentUtilsMinorVersionTest,
                        /*force_minor_version_to_M100*/ testing::Bool());

TEST_P(UserAgentUtilsMinorVersionTest, GetUserAgent) {
  const std::string ua = GetUserAgent();
  std::string major_version;
  std::string minor_version;
  EXPECT_TRUE(re2::RE2::PartialMatch(ua, kChromeProductVersionRegex,
                                     &major_version, &minor_version));
  EXPECT_EQ(major_version, version_info::GetMajorVersionNumber());
  if (ForceMinorVersionTo100()) {
    EXPECT_NE(minor_version, "100.0.0");
  } else {
    EXPECT_NE(minor_version, "0.0.0");
  }
}

}  // namespace embedder_support
