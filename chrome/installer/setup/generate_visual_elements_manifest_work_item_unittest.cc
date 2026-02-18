// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/generate_visual_elements_manifest_work_item.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

// A parameterized test harness for testing
// GenerateVisualElementsManifestWorkItem. The parameters are:
// 0: an index into a brand's install_static::kInstallModes array.
// 1: the expected manifest.
class CreateVisualElementsManifestWorkItemTest
    : public ::testing::TestWithParam<
          std::tuple<install_static::InstallConstantIndex, const char*>> {
 protected:
  CreateVisualElementsManifestWorkItemTest()
      : scoped_install_details_(/*system_level=*/false,
                                std::get<0>(GetParam())),
        expected_manifest_(std::get<1>(GetParam())),
        version_("0.0.0.0") {}

  void SetUp() override {
    // Create a temp directory for testing.
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

    version_dir_ = test_dir_.GetPath().AppendASCII(version_.GetString());
    ASSERT_TRUE(base::CreateDirectory(version_dir_));

    manifest_path_ = test_dir_.GetPath().Append(kVisualElementsManifest);
  }

  void TearDown() override {
    // Clean up test directory manually so we can fail if it leaks.
    ASSERT_TRUE(test_dir_.Delete());
  }

  // Creates a dummy test file at |path|.
  void CreateTestFile(const base::FilePath& path) {
    static constexpr char kBlah[] = "blah";
    ASSERT_TRUE(base::WriteFile(path, kBlah));
  }

  // Creates the VisualElements directory and a light asset, if testing such.
  void PrepareTestVisualElementsDirectory() {
    base::FilePath visual_elements_dir =
        version_dir_.AppendASCII(kVisualElements);
    ASSERT_TRUE(base::CreateDirectory(visual_elements_dir));
    std::wstring light_logo_file_name = base::StrCat(
        {L"Logo", install_static::InstallDetails::Get().logo_suffix(),
         L".png"});
    ASSERT_NO_FATAL_FAILURE(
        CreateTestFile(visual_elements_dir.Append(light_logo_file_name)));
  }

  // InstallDetails for this test run.
  install_static::ScopedInstallDetails scoped_install_details_;

  // The expected contents of the manifest.
  const char* const expected_manifest_;

  // A dummy version number used to create the version directory.
  const base::Version version_;

  // The temporary directory used to contain the test operations.
  base::ScopedTempDir test_dir_;

  // The path to |test_dir_|\|version_|.
  base::FilePath version_dir_;

  // The path to VisualElementsManifest.xml.
  base::FilePath manifest_path_;
};

constexpr char kExpectedPrimaryManifest[] =
    "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
    "  <VisualElements\r\n"
    "      ShowNameOnSquare150x150Logo='on'\r\n"
    "      Square150x150Logo='0.0.0.0\\VisualElements\\Logo.png'\r\n"
    "      Square70x70Logo='0.0.0.0\\VisualElements\\SmallLogo.png'\r\n"
    "      Square44x44Logo='0.0.0.0\\VisualElements\\SmallLogo.png'\r\n"
    "      ForegroundText='light'\r\n"
    "      BackgroundColor='#5F6368'/>\r\n"
    "</Application>\r\n";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kExpectedBetaManifest[] =
    "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
    "  <VisualElements\r\n"
    "      ShowNameOnSquare150x150Logo='on'\r\n"
    "      Square150x150Logo='0.0.0.0\\VisualElements\\LogoBeta.png'\r\n"
    "      Square70x70Logo='0.0.0.0\\VisualElements\\SmallLogoBeta.png'\r\n"
    "      Square44x44Logo='0.0.0.0\\VisualElements\\SmallLogoBeta.png'\r\n"
    "      ForegroundText='light'\r\n"
    "      BackgroundColor='#5F6368'/>\r\n"
    "</Application>\r\n";

constexpr char kExpectedDevManifest[] =
    "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
    "  <VisualElements\r\n"
    "      ShowNameOnSquare150x150Logo='on'\r\n"
    "      Square150x150Logo='0.0.0.0\\VisualElements\\LogoDev.png'\r\n"
    "      Square70x70Logo='0.0.0.0\\VisualElements\\SmallLogoDev.png'\r\n"
    "      Square44x44Logo='0.0.0.0\\VisualElements\\SmallLogoDev.png'\r\n"
    "      ForegroundText='light'\r\n"
    "      BackgroundColor='#5F6368'/>\r\n"
    "</Application>\r\n";

constexpr char kExpectedCanaryManifest[] =
    "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
    "  <VisualElements\r\n"
    "      ShowNameOnSquare150x150Logo='on'\r\n"
    "      Square150x150Logo='0.0.0.0\\VisualElements\\LogoCanary.png'\r\n"
    "      Square70x70Logo='0.0.0.0\\VisualElements\\SmallLogoCanary.png'\r\n"
    "      Square44x44Logo='0.0.0.0\\VisualElements\\SmallLogoCanary.png'\r\n"
    "      ForegroundText='light'\r\n"
    "      BackgroundColor='#5F6368'/>\r\n"
    "</Application>\r\n";

INSTANTIATE_TEST_SUITE_P(
    GoogleChrome,
    CreateVisualElementsManifestWorkItemTest,
    testing::Combine(testing::Values(install_static::STABLE_INDEX),
                     testing::Values(kExpectedPrimaryManifest)));
INSTANTIATE_TEST_SUITE_P(
    BetaChrome,
    CreateVisualElementsManifestWorkItemTest,
    testing::Combine(testing::Values(install_static::BETA_INDEX),
                     testing::Values(kExpectedBetaManifest)));
INSTANTIATE_TEST_SUITE_P(
    DevChrome,
    CreateVisualElementsManifestWorkItemTest,
    testing::Combine(testing::Values(install_static::DEV_INDEX),
                     testing::Values(kExpectedDevManifest)));
INSTANTIATE_TEST_SUITE_P(
    CanaryChrome,
    CreateVisualElementsManifestWorkItemTest,
    testing::Combine(testing::Values(install_static::CANARY_INDEX),
                     testing::Values(kExpectedCanaryManifest)));
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
INSTANTIATE_TEST_SUITE_P(
    ChromeForTesting,
    CreateVisualElementsManifestWorkItemTest,
    testing::Combine(
        testing::Values(install_static::GOOGLE_CHROME_FOR_TESTING_INDEX),
        testing::Values(kExpectedPrimaryManifest)));
#else
INSTANTIATE_TEST_SUITE_P(
    Chromium,
    CreateVisualElementsManifestWorkItemTest,
    testing::Combine(testing::Values(install_static::CHROMIUM_INDEX),
                     testing::Values(kExpectedPrimaryManifest)));
#endif

// Test that VisualElementsManifest.xml is created with the correct content and
// that Rollback removes it.
TEST_P(CreateVisualElementsManifestWorkItemTest, DoAndRollback) {
  ASSERT_NO_FATAL_FAILURE(PrepareTestVisualElementsDirectory());
  std::unique_ptr<WorkItem> item(new GenerateVisualElementsManifestWorkItem(
      test_dir_.GetPath(), version_));
  ASSERT_TRUE(item->Do());
  ASSERT_TRUE(base::PathExists(manifest_path_));

  std::string read_manifest;
  ASSERT_TRUE(base::ReadFileToString(manifest_path_, &read_manifest));
  ASSERT_STREQ(expected_manifest_, read_manifest.c_str());

  // Rollback should remove the generated file.
  item->Rollback();
  ASSERT_FALSE(base::PathExists(manifest_path_));
}

}  // namespace

}  // namespace installer
