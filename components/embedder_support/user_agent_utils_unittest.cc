// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/user_agent_utils.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/version_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/embedder_support/switches.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_brand_version_type.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/re2/src/re2/re2.h"

#if BUILDFLAG(IS_IOS)
#include "ui/base/device_form_factor.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.foundation.metadata.h>
#include <wrl.h>

#include "base/win/core_winrt_util.h"
#include "base/win/hstring_reference.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace embedder_support {

namespace {

// A regular expression that matches Chrome/{major_version}.{minor_version} in
// the User-Agent string, where the first capture is the {major_version} and the
// second capture is the {minor_version}.
static constexpr char kChromeProductVersionRegex[] =
    "Chrome/([0-9]+).([0-9]+).([0-9]+).([0-9]+)";

#if BUILDFLAG(IS_WIN)

// On Windows, the client hint sec-ch-ua-platform-version should be
// the highest supported version of the UniversalApiContract.
void VerifyWinPlatformVersion(std::string version) {
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

#endif  // BUILDFLAG(IS_WIN)

bool ContainsBrandVersion(const blink::UserAgentBrandList& brand_list,
                          const blink::UserAgentBrandVersion brand_version) {
  for (const auto& brand_list_entry : brand_list) {
    if (brand_list_entry == brand_version)
      return true;
  }
  return false;
}

}  // namespace

class UserAgentUtilsTest : public testing::Test {
 public:
  // The minor version in the reduced UA string is always "0.0.0".
  static constexpr char kReducedMinorVersion[] = "0.0.0";
  // The suffix added after "Chrome/<major_version>.0.0.0" and before
  // "Safari/537.36" in the user agent string when the kUseMobileUserAgent
  // switch is enabled.
  static constexpr char kMobileProductSuffix[] = "Mobile ";

  std::string GenerateExpectedUserAgent(
      const std::string& product_suffix = std::string()) {
    // This cannot be constexpr because of the runtime checks for
    // BUILDFLAG(IS_IOS).
    // This matches GetUnifiedPlatform().
    static const char* const kExpectedPlatform =
#if BUILDFLAG(IS_CHROMEOS)
        "X11; CrOS x86_64 14541.0.0";
#elif BUILDFLAG(IS_FUCHSIA)
        "Fuchsia";
#elif BUILDFLAG(IS_LINUX)
        "X11; Linux x86_64";
#elif BUILDFLAG(IS_MAC)
        "Macintosh; Intel Mac OS X 10_15_7";
#elif BUILDFLAG(IS_WIN)
        "Windows NT 10.0; Win64; x64";
#elif BUILDFLAG(IS_ANDROID)
        "Linux; Android 10; K";
#elif BUILDFLAG(IS_IOS)
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
            ? "iPad; CPU iPad OS 14_0 like Mac OS X"
            : "iPhone; CPU iPhone OS 14_0 like Mac OS X";
#else
#error Unsupported platform
#endif

    return base::StringPrintf(
        "Mozilla/5.0 (%s) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/%d.0.0.0 %sSafari/537.36",
        kExpectedPlatform, version_info::GetMajorVersionNumberAsInt(),
        product_suffix.c_str());
  }

  std::string GetUserAgentMinorVersion(const std::string& user_agent_value) {
    // A regular expression that matches Chrome/{major_version}.{minor_version}
    // in the User-Agent string, where the {minor_version} is captured.
    static constexpr char kChromeVersionRegex[] =
        "Chrome/[0-9]+\\.([0-9]+\\.[0-9]+\\.[0-9]+)";
    std::string minor_version;
    EXPECT_TRUE(re2::RE2::PartialMatch(user_agent_value, kChromeVersionRegex,
                                       &minor_version));
    return minor_version;
  }

  void VerifyGetUserAgentFunctions() {
    // GetUserAgent should return user agent depends on
    // kReduceUserAgentMinorVersion feature.
    if (base::FeatureList::IsEnabled(
            blink::features::kReduceUserAgentMinorVersion)) {
      EXPECT_EQ(GetUserAgentMinorVersion(GetUserAgent()), kReducedMinorVersion);
    } else {
      EXPECT_NE(GetUserAgentMinorVersion(GetUserAgent()), kReducedMinorVersion);
    }
  }
};

TEST_F(UserAgentUtilsTest, CustomUserAgent) {
  std::string custom_user_agent = "custom chrome user agent";
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(kUserAgent, custom_user_agent);
  ASSERT_TRUE(command_line->HasSwitch(kUserAgent));
  // Make sure user-agent API returns value correctly when user provide custom
  // user-agent.
  EXPECT_EQ(GetUserAgent(), custom_user_agent);

  base::test::ScopedFeatureList scoped_feature_list;
  {
    auto metadata = GetUserAgentMetadata();

    // Verify low-entropy client hints aren't empty.
    const std::string major_version = version_info::GetMajorVersionNumber();
    const blink::UserAgentBrandVersion chromium_brand_version = {"Chromium",
                                                                 major_version};
    EXPECT_TRUE(ContainsBrandVersion(metadata.brand_version_list,
                                     chromium_brand_version));
    EXPECT_NE("", metadata.platform);

    // Verify high-entropy client hints are empty, take platform version as
    // an example to verify.
    EXPECT_EQ("", metadata.platform_version);
  }

  scoped_feature_list.InitAndEnableFeature(blink::features::kUACHOverrideBlank);
  {
    // Make sure return blank values for GetUserAgentMetadata().
    EXPECT_EQ(blink::UserAgentMetadata::Marshal(blink::UserAgentMetadata()),
              blink::UserAgentMetadata::Marshal(GetUserAgentMetadata()));
  }
}

TEST_F(UserAgentUtilsTest, InvalidCustomUserAgent) {
  std::string custom_user_agent = "custom \rchrome user agent";
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(kUserAgent, custom_user_agent);
  ASSERT_TRUE(command_line->HasSwitch(kUserAgent));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kReduceUserAgentMinorVersion);

  EXPECT_EQ(GetUserAgent(), GenerateExpectedUserAgent());
}

TEST_F(UserAgentUtilsTest, UserAgentStringReduced) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kReduceUserAgentMinorVersion);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Verify the correct user agent is returned when the UseMobileUserAgent
  // command line flag is present.
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();

  // Verify the mobile user agent string is not returned when not using a mobile
  // user agent.
  ASSERT_FALSE(command_line->HasSwitch(kUseMobileUserAgent));
  EXPECT_EQ(GetUserAgent(), GenerateExpectedUserAgent());

  // Verify the mobile user agent string is returned when using a mobile user
  // agent.
  command_line->AppendSwitch(kUseMobileUserAgent);
  ASSERT_TRUE(command_line->HasSwitch(kUseMobileUserAgent));
  EXPECT_EQ(GetUserAgent(), GenerateExpectedUserAgent(kMobileProductSuffix));
#else
  EXPECT_EQ(GetUserAgent(), GenerateExpectedUserAgent());
#endif
}

TEST_F(UserAgentUtilsTest, UserAgentStringFull) {
  base::test::ScopedFeatureList scoped_feature_list;

  // Verify that GetUserAgent() returns the correct user agent string when
  // kReduceUserAgentMinorVersion turns on.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {blink::features::kReduceUserAgentMinorVersion}, {});
  { VerifyGetUserAgentFunctions(); }

  // Verify that GetUserAgent() returns the correct user agent string when
  // kReduceUserAgentMinorVersion turns off.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {}, {blink::features::kReduceUserAgentMinorVersion});
  { VerifyGetUserAgentFunctions(); }

  // Verify that GetUserAgent() returns the correct user agent string without
  // explicit features turned on.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures({}, {});
  { VerifyGetUserAgentFunctions(); }
}

TEST_F(UserAgentUtilsTest, UserAgentMetadata) {
  auto metadata = GetUserAgentMetadata();

  const std::string major_version = version_info::GetMajorVersionNumber();
  const std::string full_version(version_info::GetVersionNumber());

  // According to spec, Sec-CH-UA should contain what project the browser is
  // based on (i.e. Chromium in this case) as well as the actual product.
  // In CHROMIUM_BRANDING builds this will check chromium twice. That should be
  // ok though.

  const blink::UserAgentBrandVersion chromium_brand_version = {"Chromium",
                                                               major_version};
  const blink::UserAgentBrandVersion product_brand_version = {
      std::string(version_info::GetProductName()), major_version};

  EXPECT_TRUE(ContainsBrandVersion(metadata.brand_version_list,
                                   chromium_brand_version));
  EXPECT_TRUE(
      ContainsBrandVersion(metadata.brand_version_list, product_brand_version));

  // verify full version list
  const blink::UserAgentBrandVersion chromium_brand_full_version = {
      "Chromium", full_version};
  const blink::UserAgentBrandVersion product_brand_full_version = {
      std::string(version_info::GetProductName()), full_version};

  EXPECT_TRUE(ContainsBrandVersion(metadata.brand_full_version_list,
                                   chromium_brand_full_version));
  EXPECT_TRUE(ContainsBrandVersion(metadata.brand_full_version_list,
                                   product_brand_full_version));
  EXPECT_EQ(metadata.full_version, full_version);

#if BUILDFLAG(IS_WIN)
  VerifyWinPlatformVersion(metadata.platform_version);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA)
  EXPECT_EQ(metadata.platform_version, "");
#else
  int32_t major, minor, bugfix = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
  EXPECT_EQ(metadata.platform_version,
            base::StringPrintf("%d.%d.%d", major, minor, bugfix));
#endif
  // This makes sure no extra information is added to the platform version.
  EXPECT_EQ(metadata.platform_version.find(";"), std::string::npos);
  // If you're here because your change to GetOSType broke this test, it likely
  // means that GetPlatformForUAMetadata needs a new special case to prevent
  // breaking client hints. Check with the code owners for further guidance.
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(metadata.platform, "Windows");
#elif BUILDFLAG(IS_IOS)
  EXPECT_EQ(metadata.platform, "iOS");
#elif BUILDFLAG(IS_MAC)
  EXPECT_EQ(metadata.platform, "macOS");
#elif BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(metadata.platform, "Chrome OS");
#else
  EXPECT_EQ(metadata.platform, "Chromium OS");
#endif
#elif BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(metadata.platform, "Android");
#elif BUILDFLAG(IS_LINUX)
  EXPECT_EQ(metadata.platform, "Linux");
#elif BUILDFLAG(IS_FREEBSD)
  EXPECT_EQ(metadata.platform, "FreeBSD");
#elif BUILDFLAG(IS_OPENBSD)
  EXPECT_EQ(metadata.platform, "OpenBSD");
#elif BUILDFLAG(IS_SOLARIS)
  EXPECT_EQ(metadata.platform, "Solaris");
#elif BUILDFLAG(IS_FUCHSIA)
  EXPECT_EQ(metadata.platform, "Fuchsia");
#else
  EXPECT_EQ(metadata.platform, "Unknown");
#endif
  EXPECT_EQ(metadata.architecture, GetCpuArchitecture());
  EXPECT_EQ(metadata.model, BuildModelInfo());
  EXPECT_EQ(metadata.bitness, GetCpuBitness());
  EXPECT_EQ(metadata.wow64, IsWoW64());
  std::vector<std::string> expected_form_factors = {
      metadata.mobile ? "Mobile" : "Desktop"};
  EXPECT_EQ(metadata.form_factors, expected_form_factors);

  // Verify only populate low-entropy client hints.
  metadata = GetUserAgentMetadata(true);
  EXPECT_TRUE(ContainsBrandVersion(metadata.brand_version_list,
                                   chromium_brand_version));
  EXPECT_TRUE(
      ContainsBrandVersion(metadata.brand_version_list, product_brand_version));
  // High entropy should be empty.
  EXPECT_TRUE(metadata.brand_full_version_list.empty());
  EXPECT_TRUE(metadata.full_version.empty());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(UserAgentUtilsTest, UserAgentMetadataForXrDevice) {
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }

  base::android::device_info::set_is_xr_for_testing();
  EXPECT_EQ(base::android::device_info::is_xr(), true);

  // By default (both flags disabled), it should return Linux x86_64.
  EXPECT_EQ(GetUnifiedPlatformForTesting(), "X11; Linux x86_64");

  auto metadata = GetUserAgentMetadata();
  EXPECT_EQ(metadata.platform, "Linux");

  // Verify the XR specific info set when both flags are disabled.
  // TODO(crbug.com/433345971): Avoid hardcoding x86 when both flags are
  // disabled.
  EXPECT_EQ(metadata.architecture, "x86");
  EXPECT_EQ(metadata.bitness, "64");
  EXPECT_EQ(metadata.mobile, false);
  EXPECT_EQ(metadata.platform_version, "");

  // Enable the flag to spoof as ChromeOS.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{blink::features::kAndroidDesktopUASpoofAsChromeOS,
                              blink::features::kAndroidDesktopUACPUArch},
        /*disabled_features=*/{});

    std::string expected_arch = GetCpuArchitecture();
    // Get unified platform of the user-agent on xr device.
    EXPECT_EQ(GetUnifiedPlatformForTesting(), "X11; CrOS x86_64 14541.0.0");

    auto metadata_cros = GetUserAgentMetadata();
    EXPECT_EQ(metadata_cros.platform, "Chrome OS");
    EXPECT_EQ(metadata_cros.architecture, expected_arch);
    EXPECT_EQ(metadata_cros.bitness, "64");
  }

  // Verify user-agent client-hints form-factors
  std::vector<std::string> expected_form_factors = {"Desktop", "XR"};
  EXPECT_EQ(metadata.form_factors, expected_form_factors);

  // The kAndroidDesktopUAPlatform changes XR devices platform client hint to
  // Android, and reports the real OS version instead of an empty platform
  // version.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        blink::features::kAndroidDesktopUAPlatform);
    auto metadata_with_feature = GetUserAgentMetadata();
    EXPECT_EQ(metadata_with_feature.platform, "Android");
    int32_t major, minor, bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    EXPECT_EQ(metadata_with_feature.platform_version,
              base::StringPrintf("%d.%d.%d", major, minor, bugfix));
    std::string expected_arch = GetCpuArchitecture();
    EXPECT_EQ(metadata_with_feature.architecture, expected_arch);
    EXPECT_EQ(metadata_with_feature.bitness, "64");
  }

  // Restore the device info.
  base::android::device_info::reset_is_xr_for_testing();
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(UserAgentUtilsTest, GenerateBrandVersionListUnbranded) {
  blink::UserAgentMetadata metadata;
  metadata.brand_version_list = GenerateBrandVersionList(
      84, std::nullopt, "84", blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list =
      GenerateBrandVersionList(84, std::nullopt, "84.0.0.0",
                               blink::UserAgentBrandVersionType::kFullVersion);
  // 1. verify major version
  std::string brand_list = metadata.SerializeBrandMajorVersionList();
  EXPECT_EQ(R"("Not;A=Brand";v="8", "Chromium";v="84")", brand_list);
  // 2. verify full version
  std::string brand_list_w_fv = metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(R"("Not;A=Brand";v="8.0.0.0", "Chromium";v="84.0.0.0")",
            brand_list_w_fv);
}

TEST_F(UserAgentUtilsTest, GenerateBrandVersionListUnbrandedVerifySeedChanges) {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list = GenerateBrandVersionList(
      84, std::nullopt, "84", blink::UserAgentBrandVersionType::kMajorVersion);
  // Capture the serialized brand lists with version 84 as the seed.
  std::string brand_list = metadata.SerializeBrandMajorVersionList();
  std::string brand_list_w_fv = metadata.SerializeBrandFullVersionList();

  metadata.brand_version_list = GenerateBrandVersionList(
      85, std::nullopt, "85", blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list =
      GenerateBrandVersionList(85, std::nullopt, "85.0.0.0",
                               blink::UserAgentBrandVersionType::kFullVersion);

  // Make sure the lists are different for different seeds (84 vs 85).
  // 1. verify major version
  std::string brand_list_diff = metadata.SerializeBrandMajorVersionList();
  EXPECT_EQ(R"("Chromium";v="85", "Not=A?Brand";v="99")", brand_list_diff);
  EXPECT_NE(brand_list, brand_list_diff);
  // 2.verify full version
  std::string brand_list_diff_w_fv = metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(R"("Chromium";v="85.0.0.0", "Not=A?Brand";v="99.0.0.0")",
            brand_list_diff_w_fv);
  EXPECT_NE(brand_list_w_fv, brand_list_diff_w_fv);
}

TEST_F(UserAgentUtilsTest, GenerateBrandVersionListAdditionalBrandVersions) {
  blink::UserAgentMetadata metadata;
  // The GREASE generation algorithm should respond to experiment overrides.
  blink::UserAgentBrandVersion additional_brand_major_versions = {"Add Brand",
                                                                  "1"};
  blink::UserAgentBrandVersion additional_brand_full_versions = {"Add Brand",
                                                                 "1.0.0.0"};

  // 1. Without product brand.
  metadata.brand_version_list = GenerateBrandVersionList(
      84, std::nullopt, "84", blink::UserAgentBrandVersionType::kMajorVersion,
      additional_brand_major_versions);
  metadata.brand_full_version_list =
      GenerateBrandVersionList(84, std::nullopt, "84.0.0.0",
                               blink::UserAgentBrandVersionType::kFullVersion,
                               additional_brand_full_versions);
  // Verify major version and full version.
  EXPECT_EQ(base::StrCat({"\"Not;A=Brand\";v=\"8\", ",
                          "\"Chromium\";v=\"84\", ", "\"Add Brand\";v=\"1\""}),
            metadata.SerializeBrandMajorVersionList());
  EXPECT_EQ(base::StrCat({"\"Not;A=Brand\";v=\"8.0.0.0\", ",
                          "\"Chromium\";v=\"84.0.0.0\", ",
                          "\"Add Brand\";v=\"1.0.0.0\""}),
            metadata.SerializeBrandFullVersionList());

  // 2. With product brand
  metadata.brand_version_list =
      GenerateBrandVersionList(84, "Product Brand", "84",
                               blink::UserAgentBrandVersionType::kMajorVersion,
                               additional_brand_major_versions);
  metadata.brand_full_version_list =
      GenerateBrandVersionList(84, "Product Brand", "84.0.0.0",
                               blink::UserAgentBrandVersionType::kFullVersion,
                               additional_brand_full_versions);
  // Verify major version and full version.
  EXPECT_EQ(
      base::StrCat({"\"Chromium\";v=\"84\", ", "\"Product Brand\";v=\"84\", ",
                    "\"Not;A=Brand\";v=\"8\", ", "\"Add Brand\";v=\"1\""}),
      metadata.SerializeBrandMajorVersionList());
  EXPECT_EQ(base::StrCat({"\"Chromium\";v=\"84.0.0.0\", ",
                          "\"Product Brand\";v=\"84.0.0.0\", ",
                          "\"Not;A=Brand\";v=\"8.0.0.0\", ",
                          "\"Add Brand\";v=\"1.0.0.0\""}),
            metadata.SerializeBrandFullVersionList());

  // 3. With product brand and different seed.
  metadata.brand_version_list =
      GenerateBrandVersionList(86, "Product Brand", "84",
                               blink::UserAgentBrandVersionType::kMajorVersion,
                               additional_brand_major_versions);
  metadata.brand_full_version_list =
      GenerateBrandVersionList(86, "Product Brand", "84.0.0.0",
                               blink::UserAgentBrandVersionType::kFullVersion,
                               additional_brand_full_versions);
  // Verify major version and full version.
  EXPECT_EQ(
      base::StrCat({"\"Product Brand\";v=\"84\", ", "\"Chromium\";v=\"84\", ",
                    "\"Not?A_Brand\";v=\"24\", ", "\"Add Brand\";v=\"1\""}),
      metadata.SerializeBrandMajorVersionList());
  EXPECT_EQ(base::StrCat({"\"Product Brand\";v=\"84.0.0.0\", ",
                          "\"Chromium\";v=\"84.0.0.0\", ",
                          "\"Not?A_Brand\";v=\"24.0.0.0\", ",
                          "\"Add Brand\";v=\"1.0.0.0\""}),
            metadata.SerializeBrandFullVersionList());

  // 6. Default API calls and make sure it contains the additional brand.
  metadata.brand_version_list =
      GetUserAgentBrandMajorVersionList(additional_brand_major_versions);
  metadata.brand_full_version_list =
      GetUserAgentBrandFullVersionList(additional_brand_full_versions);
  EXPECT_THAT(metadata.SerializeBrandMajorVersionList(),
              testing::HasSubstr("Add Brand"));
  EXPECT_THAT(metadata.SerializeBrandFullVersionList(),
              testing::HasSubstr("Add Brand"));
  // Confirm version is correct generated.
  EXPECT_THAT(metadata.SerializeBrandMajorVersionList(),
              testing::HasSubstr("v=\"" +
                                 version_info::GetMajorVersionNumber() + "\""));
  EXPECT_THAT(
      metadata.SerializeBrandFullVersionList(),
      testing::HasSubstr("v=\"" +
                         std::string(version_info::GetVersionNumber()) + "\""));
}

TEST_F(UserAgentUtilsTest, GenerateBrandVersionListWithBrand) {
  blink::UserAgentMetadata metadata;
  metadata.brand_version_list =
      GenerateBrandVersionList(84, "Totally A Brand", "84",
                               blink::UserAgentBrandVersionType::kMajorVersion);
  metadata.brand_full_version_list =
      GenerateBrandVersionList(84, "Totally A Brand", "84.0.0.0",
                               blink::UserAgentBrandVersionType::kFullVersion);
  // 1. verify major version
  std::string brand_list_w_brand = metadata.SerializeBrandMajorVersionList();
  EXPECT_EQ(
      R"("Not;A=Brand";v="8", "Chromium";v="84", "Totally A Brand";v="84")",
      brand_list_w_brand);
  // 2. verify full version
  std::string brand_list_w_brand_fv = metadata.SerializeBrandFullVersionList();
  EXPECT_EQ(base::StrCat({"\"Not;A=Brand\";v=\"8.0.0.0\", ",
                          "\"Chromium\";v=\"84.0.0.0\", ",
                          "\"Totally A Brand\";v=\"84.0.0.0\""}),
            brand_list_w_brand_fv);
}

TEST_F(UserAgentUtilsTest, GenerateBrandVersionListInvalidSeed) {
  // Should DCHECK on negative numbers
  EXPECT_DCHECK_DEATH(GenerateBrandVersionList(
      -1, std::nullopt, "99", blink::UserAgentBrandVersionType::kMajorVersion));
  EXPECT_DCHECK_DEATH(
      GenerateBrandVersionList(-1, std::nullopt, "99.0.0.0",
                               blink::UserAgentBrandVersionType::kFullVersion));
}

TEST_F(UserAgentUtilsTest, GetGreasedUserAgentBrandVersion) {
  blink::UserAgentBrandVersion greased_bv = GetGreasedUserAgentBrandVersion(
      84, blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "Not;A=Brand");
  EXPECT_EQ(greased_bv.version, "8");

  greased_bv = GetGreasedUserAgentBrandVersion(
      84, blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "Not;A=Brand");
  EXPECT_EQ(greased_bv.version, "8.0.0.0");
}

TEST_F(UserAgentUtilsTest, GetGreasedUserAgentBrandVersionFullVersions) {
  blink::UserAgentBrandVersion greased_bv = GetGreasedUserAgentBrandVersion(
      86, blink::UserAgentBrandVersionType::kMajorVersion);
  EXPECT_EQ(greased_bv.brand, "Not?A_Brand");
  EXPECT_EQ(greased_bv.version, "24");

  greased_bv = GetGreasedUserAgentBrandVersion(
      86, blink::UserAgentBrandVersionType::kFullVersion);
  EXPECT_EQ(greased_bv.brand, "Not?A_Brand");
  EXPECT_EQ(greased_bv.version, "24.0.0.0");
}

TEST_F(UserAgentUtilsTest, GetGreasedUserAgentBrandVersionNoLeadingWhitespace) {
  blink::UserAgentBrandVersion greased_bv;
  // Go up to 110 based on the 11 total chars * 10 possible first chars.
  for (int i = 0; i < 110; i++) {
    // Regardless of the major version seed, the spec calls for no leading
    // whitespace in the brand.
    greased_bv = GetGreasedUserAgentBrandVersion(
        i, blink::UserAgentBrandVersionType::kMajorVersion);
    EXPECT_NE(greased_bv.brand[0], ' ');

    greased_bv = GetGreasedUserAgentBrandVersion(
        i, blink::UserAgentBrandVersionType::kFullVersion);
    EXPECT_NE(greased_bv.brand[0], ' ');
  }
}

TEST_F(UserAgentUtilsTest, GetProductAndVersion) {
  std::string product;
  std::string major_version;
  std::string minor_version;
  std::string build_version;
  std::string patch_version;

  // Feature kReduceUserAgentMinorVersion disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          blink::features::kReduceUserAgentMinorVersion});

  product = GetProductAndVersion();
  EXPECT_TRUE(re2::RE2::FullMatch(product, kChromeProductVersionRegex,
                                  &major_version, &minor_version,
                                  &build_version));
  EXPECT_EQ(major_version, version_info::GetMajorVersionNumber());
  EXPECT_EQ(minor_version, "0");
  EXPECT_NE(build_version, "0");
  // Patch version cannot be tested as it would be set in a release branch.

  // Feature kReduceUserAgentMinorVersion enabled with version.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{blink::features::kReduceUserAgentMinorVersion},
      /*disabled_features=*/{});
  product = GetProductAndVersion();
  EXPECT_TRUE(re2::RE2::FullMatch(product, kChromeProductVersionRegex,
                                  &major_version, &minor_version,
                                  &build_version, &patch_version));
  EXPECT_EQ(major_version, version_info::GetMajorVersionNumber());
  EXPECT_EQ(minor_version, "0");
  EXPECT_EQ(build_version, "0");
  EXPECT_EQ(patch_version, "0");
}

TEST_F(UserAgentUtilsTest, HeadlessUserAgent) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(kHeadless);
  ASSERT_TRUE(command_line->HasSwitch(kHeadless));

  // In headless mode product name should have the Headless prefix.
  EXPECT_THAT(GetUserAgent(), testing::HasSubstr("HeadlessChrome/"));
}

namespace {

struct BuildOSCpuInfoTestCases {
  std::string os_version;
  std::string cpu_type;
  std::string expected_os_cpu_info;
};

}  // namespace

TEST_F(UserAgentUtilsTest, BuildOSCpuInfoFromOSVersionAndCpuType) {
  // clang-format off
  const BuildOSCpuInfoTestCases test_cases[] = {
#if BUILDFLAG(IS_WIN)
    // On Windows, it's possible to have an empty string for CPU type.
    {
        /*os_version=*/"10.0",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"Windows NT 10.0",
    },
    {
        /*os_version=*/"10.0",
        /*cpu_type=*/"WOW64",
        /*expected_os_cpu_info=*/"Windows NT 10.0; WOW64",
    },
    {
        /*os_version=*/"10.0",
        /*cpu_type=*/"Win64; x64",
        /*expected_os_cpu_info=*/"Windows NT 10.0; Win64; x64",
    },
    {
        /*os_version=*/"7.0",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"Windows NT 7.0",
    },
    // These cases should never happen in real life, but may be useful to detect
    // changes when things are refactored.
    {
        /*os_version=*/"",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"Windows NT ",
    },
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"Windows NT VERSION; CPU TYPE",
    },
#elif BUILDFLAG(IS_MAC)
    {
        /*os_version=*/"10_15_4",
        /*cpu_type=*/"Intel",
        /*expected_os_cpu_info=*/"Intel Mac OS X 10_15_4",
    },
    // These cases should never happen in real life, but may be useful to detect
    // changes when things are refactored.
    {
        /*os_version=*/"",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/" Mac OS X ",
    },
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"CPU TYPE Mac OS X VERSION",
    },
#elif BUILDFLAG(IS_CHROMEOS)
    {
        /*os_version=*/"4537.56.0",
        /*cpu_type=*/"armv7l",
        /*expected_os_cpu_info=*/"CrOS armv7l 4537.56.0",
    },
    // These cases should never happen in real life, but may be useful to detect
    // changes when things are refactored.
    {
        /*os_version=*/"",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"CrOS  ",
    },
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"CrOS CPU TYPE VERSION",
    },
#elif BUILDFLAG(IS_ANDROID)
    {
        /*os_version=*/"7.1.1",
        /*cpu_type=*/"UNUSED",
        /*expected_os_cpu_info=*/"Android 7.1.1",
    },
    // These cases should never happen in real life, but may be useful to detect
    // changes when things are refactored.
    {
        /*os_version=*/"",
        /*cpu_type=*/"",
        /*expected_os_cpu_info=*/"Android ",
    },
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"Android VERSION",
    },
#elif BUILDFLAG(IS_FUCHSIA)
    {
        /*os_version=*/"VERSION",
        /*cpu_type=*/"CPU TYPE",
        /*expected_os_cpu_info=*/"Fuchsia",
    },
#endif
  };
  // clang-format on

  for (const auto& test_case : test_cases) {
    const std::string os_cpu_info = BuildOSCpuInfoFromOSVersionAndCpuType(
        test_case.os_version, test_case.cpu_type);
    EXPECT_EQ(os_cpu_info, test_case.expected_os_cpu_info);
  }
}

TEST_F(UserAgentUtilsTest, GetCpuArchitecture) {
  std::string arch = GetCpuArchitecture();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ("", arch);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_POSIX)
  EXPECT_TRUE("arm" == arch || "x86" == arch);
#else
#error Unsupported platform
#endif
}

TEST_F(UserAgentUtilsTest, GetCpuBitness) {
  std::string bitness = GetCpuBitness();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ("", bitness);
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_POSIX)
  EXPECT_TRUE("32" == bitness || "64" == bitness);
#else
#error Unsupported platform
#endif
}

}  // namespace embedder_support
