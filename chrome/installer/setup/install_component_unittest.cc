// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_component.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/version.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crx_file/crx_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

// The developer key ID of "valid_publisher.crx3".
constexpr char kTestDeveloperCrxId[] = "ojjgnpkioondelmggbekfhllhdaimnho";

// The SHA256 of the developer SubjectPublicKeyInfo of "valid_publisher.crx3".
constexpr uint8_t kTestDeveloperPublicKeySHA256[32] = {
    0xe9, 0x96, 0xdf, 0xa8, 0xee, 0xd3, 0x4b, 0xc6, 0x61, 0x4a, 0x57,
    0xbb, 0x73, 0x08, 0xcd, 0x7e, 0x51, 0x9b, 0xcc, 0x69, 0x08, 0x41,
    0xe1, 0x96, 0x9f, 0x7c, 0xb1, 0x73, 0xef, 0x16, 0x80, 0x0a};

constexpr ComponentConfig kTestComponents[] = {
    {"sthset", kTestDeveloperPublicKeySHA256},
};

base::FilePath GetTestCrxPath(
    const std::string& filename = "valid_publisher.crx3") {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  return test_data_root.Append(FILE_PATH_LITERAL("components"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("crx_file"))
      .AppendASCII(filename);
}

}  // namespace

TEST(InstallComponentTest, GetComponentVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Valid version directory with manifest.json.
  base::FilePath valid_dir = temp_dir.GetPath().AppendASCII("1.2.3.4");
  ASSERT_TRUE(base::CreateDirectory(valid_dir));
  ASSERT_TRUE(base::WriteFile(valid_dir.AppendASCII("manifest.json"), "dummy"));
  EXPECT_EQ(GetComponentVersion(valid_dir), base::Version("1.2.3.4"));

  // Valid version directory without manifest.json.
  base::FilePath no_manifest_dir = temp_dir.GetPath().AppendASCII("2.0.0.0");
  ASSERT_TRUE(base::CreateDirectory(no_manifest_dir));
  EXPECT_EQ(GetComponentVersion(no_manifest_dir), std::nullopt);

  // Invalid version directory name with manifest.json.
  base::FilePath invalid_name_dir =
      temp_dir.GetPath().AppendASCII("not_a_version");
  ASSERT_TRUE(base::CreateDirectory(invalid_name_dir));
  ASSERT_TRUE(
      base::WriteFile(invalid_name_dir.AppendASCII("manifest.json"), "dummy"));
  EXPECT_EQ(GetComponentVersion(invalid_name_dir), std::nullopt);

  // Non-existent directory.
  base::FilePath non_existent_dir =
      temp_dir.GetPath().AppendASCII("non_existent");
  EXPECT_EQ(GetComponentVersion(non_existent_dir), std::nullopt);
}

TEST(InstallComponentTest, FindHighestComponentVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath component_root = temp_dir.GetPath().AppendASCII("Component");

  // Non-existent root directory.
  EXPECT_EQ(FindHighestComponentVersion(component_root), std::nullopt);

  ASSERT_TRUE(base::CreateDirectory(component_root));

  // Empty root directory.
  EXPECT_EQ(FindHighestComponentVersion(component_root), std::nullopt);

  // Populate multiple version directories.
  base::FilePath v1 = component_root.AppendASCII("1.0.0.0");
  base::FilePath v2 = component_root.AppendASCII("2.0.0.0");
  base::FilePath v3_corrupted = component_root.AppendASCII("3.0.0.0");
  base::FilePath invalid_dir = component_root.AppendASCII("invalid_name");

  ASSERT_TRUE(base::CreateDirectory(v1));
  ASSERT_TRUE(base::WriteFile(v1.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(v2));
  ASSERT_TRUE(base::WriteFile(v2.AppendASCII("manifest.json"), "dummy"));

  // v3_corrupted has higher version name but lacks manifest.json.
  ASSERT_TRUE(base::CreateDirectory(v3_corrupted));

  // invalid_dir has manifest.json but not a valid version name.
  ASSERT_TRUE(base::CreateDirectory(invalid_dir));
  ASSERT_TRUE(
      base::WriteFile(invalid_dir.AppendASCII("manifest.json"), "dummy"));

  // Should return 2.0.0.0 (the highest valid version).
  EXPECT_EQ(FindHighestComponentVersion(component_root),
            base::Version("2.0.0.0"));
}

TEST(InstallComponentTest, DeleteInvalidComponentDirectories) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath component_root = temp_dir.GetPath().AppendASCII("Component");
  ASSERT_TRUE(base::CreateDirectory(component_root));

  base::FilePath v1 = component_root.AppendASCII("1.0.0.0");
  base::FilePath v2 = component_root.AppendASCII("2.0.0.0");
  base::FilePath v3 = component_root.AppendASCII("3.0.0.0");
  base::FilePath v4_corrupted = component_root.AppendASCII("4.0.0.0");
  base::FilePath invalid_dir = component_root.AppendASCII("corrupted_dir");

  ASSERT_TRUE(base::CreateDirectory(v1));
  ASSERT_TRUE(base::WriteFile(v1.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(v2));
  ASSERT_TRUE(base::WriteFile(v2.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(v3));
  ASSERT_TRUE(base::WriteFile(v3.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(v4_corrupted));
  ASSERT_TRUE(base::WriteFile(v4_corrupted.AppendASCII("other.dll"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(invalid_dir));

  ASSERT_TRUE(base::PathExists(v1));
  ASSERT_TRUE(base::PathExists(v2));
  ASSERT_TRUE(base::PathExists(v3));
  ASSERT_TRUE(base::PathExists(v4_corrupted));
  ASSERT_TRUE(base::PathExists(invalid_dir));

  // Keep version 2.0.0.0.
  // v1 (< 2.0.0.0), v4_corrupted (missing manifest), and invalid_dir should be
  // deleted. v2 (== 2.0.0.0) and v3 (> 2.0.0.0) should remain.
  DeleteInvalidComponentDirectories(component_root, base::Version("2.0.0.0"));

  EXPECT_FALSE(base::PathExists(v1));
  EXPECT_TRUE(base::PathExists(v2));
  EXPECT_TRUE(base::PathExists(v3));
  EXPECT_FALSE(base::PathExists(v4_corrupted));
  EXPECT_FALSE(base::PathExists(invalid_dir));
}

TEST(InstallComponentTest, DeleteInvalidComponentDirectoriesLockedFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath component_root = temp_dir.GetPath().AppendASCII("Component");
  ASSERT_TRUE(base::CreateDirectory(component_root));

  base::FilePath v1 = component_root.AppendASCII("1.0.0.0");
  ASSERT_TRUE(base::CreateDirectory(v1));
  base::FilePath dll_path = v1.AppendASCII("test.dll");
  ASSERT_TRUE(base::WriteFile(dll_path, "dummy"));

  ASSERT_TRUE(base::PathExists(dll_path));

  {
    // Lock the file.
    base::File file(dll_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                  base::File::FLAG_WIN_EXCLUSIVE_READ |
                                  base::File::FLAG_WIN_EXCLUSIVE_WRITE);
    ASSERT_TRUE(file.IsValid());

    // Attempt to delete. It should skip v1 because test.dll is locked.
    DeleteInvalidComponentDirectories(component_root, base::Version("2.0.0.0"));

    // v1 should still exist along with the locked file.
    EXPECT_TRUE(base::PathExists(v1));
    EXPECT_TRUE(base::PathExists(dll_path));
  }

  // Lock is released now. Attempt to delete again.
  DeleteInvalidComponentDirectories(component_root, base::Version("2.0.0.0"));

  // v1 should be deleted.
  EXPECT_FALSE(base::PathExists(v1));
}

TEST(InstallComponentTest, AntiDowngrade) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  base::FilePath component_root =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId);
  base::CreateDirectory(component_root);

  // Install versions.
  base::FilePath v100 = component_root.AppendASCII("100");
  base::FilePath v500 = component_root.AppendASCII("500");
  base::CreateDirectory(v100);
  base::CreateDirectory(v500);
  // Manifest.json is required to consider a directory a valid version.
  base::WriteFile(v100.AppendASCII("manifest.json"), "dummy");
  base::WriteFile(v500.AppendASCII("manifest.json"), "dummy");

  ASSERT_TRUE(base::PathExists(v100));
  ASSERT_TRUE(base::PathExists(v500));

  // Attempt to install an older version (394).
  {
    base::FilePath src_file = GetTestCrxPath();
    ASSERT_TRUE(base::PathExists(src_file));

    EXPECT_EQ(INSTALL_COMPONENT_ALREADY_EXISTS,
              InstallComponentForTesting(
                  src_file, installer_state, kTestComponents,
                  crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));

    // Existing versions remain untouched when an installation attempt is
    // aborted due to anti-downgrade.
    EXPECT_TRUE(base::PathExists(v100));
    EXPECT_TRUE(base::PathExists(v500));
  }
}

TEST(InstallComponentTest, UpgradeDeletesOlderVersions) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  base::FilePath component_root =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId);
  base::CreateDirectory(component_root);

  // Install older versions.
  base::FilePath v100 = component_root.AppendASCII("100");
  base::FilePath v200 = component_root.AppendASCII("200");
  base::CreateDirectory(v100);
  base::CreateDirectory(v200);
  base::WriteFile(v100.AppendASCII("manifest.json"), "dummy");
  base::WriteFile(v200.AppendASCII("manifest.json"), "dummy");

  ASSERT_TRUE(base::PathExists(v100));
  ASSERT_TRUE(base::PathExists(v200));

  // Install version 394.
  base::FilePath src_file = GetTestCrxPath();
  ASSERT_TRUE(base::PathExists(src_file));

  EXPECT_EQ(INSTALL_COMPONENT_SUCCESS,
            InstallComponentForTesting(
                src_file, installer_state, kTestComponents,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));

  // Older versions should be deleted upon successful installation of 394.
  EXPECT_FALSE(base::PathExists(v100));
  EXPECT_FALSE(base::PathExists(v200));

  // 394 should exist.
  base::FilePath v394 = component_root.AppendASCII("394");
  EXPECT_TRUE(base::PathExists(v394));
}

TEST(InstallComponentTest, ManifestNameMismatch) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  base::FilePath src_file = GetTestCrxPath();
  ASSERT_TRUE(base::PathExists(src_file));

  static constexpr ComponentConfig kMismatchedComponents[] = {
      {"mismatched_manifest_name", kTestDeveloperPublicKeySHA256},
  };

  EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT,
            InstallComponentForTesting(
                src_file, installer_state, kMismatchedComponents,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));
}

TEST(InstallComponentTest, PublicKeyMismatch) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  base::FilePath src_file = GetTestCrxPath();
  ASSERT_TRUE(base::PathExists(src_file));

  // Same manifest name, but a mismatched public key hash.
  static constexpr uint8_t kWrongPublicKeySHA256[32] = {0x12, 0x34};
  static constexpr ComponentConfig kWrongKeyComponents[] = {
      {"sthset", kWrongPublicKeySHA256},
  };

  EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT,
            InstallComponentForTesting(
                src_file, installer_state, kWrongKeyComponents,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));
}

TEST(InstallComponentTest, SignatureFailure) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  // A CRX with a test publisher proof must fail when production publisher
  // proof is required.
  base::FilePath test_publisher_crx =
      GetTestCrxPath("valid_test_publisher.crx3");
  ASSERT_TRUE(base::PathExists(test_publisher_crx));
  EXPECT_EQ(INSTALL_COMPONENT_FAILED_SIGNATURE,
            InstallComponentForTesting(
                test_publisher_crx, installer_state, kTestComponents,
                crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF));

  // A CRX without publisher proof must fail when publisher proof is required.
  base::FilePath no_publisher_crx = GetTestCrxPath("valid_no_publisher.crx3");
  ASSERT_TRUE(base::PathExists(no_publisher_crx));
  EXPECT_EQ(INSTALL_COMPONENT_FAILED_SIGNATURE,
            InstallComponentForTesting(
                no_publisher_crx, installer_state, kTestComponents,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));
}

TEST(InstallComponentTest, UnsupportedComponent) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  base::FilePath src_file = GetTestCrxPath();
  ASSERT_TRUE(base::PathExists(src_file));

  // Production InstallComponent only supports production components
  // (e.g. kPlatformRuntimeCrxId), so the test CRX is unsupported.
  EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT,
            InstallComponent(src_file, installer_state));

  // Omitting the developer key from supported components must reject the CRX.
  EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT,
            InstallComponentForTesting(
                src_file, installer_state,
                /*supported_components=*/{},
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));
}

TEST(InstallComponentTest, CleanupCorruptedOrEmptyDirectories) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  base::FilePath component_root =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId);
  base::CreateDirectory(component_root);

  // Create a corrupted directory pretending to be a newer version (500) but
  // lacking manifest.json.
  base::FilePath v500_corrupted = component_root.AppendASCII("500");
  base::CreateDirectory(v500_corrupted);
  base::WriteFile(v500_corrupted.AppendASCII("readme.txt"), "corrupted");

  ASSERT_TRUE(base::PathExists(v500_corrupted));

  // Attempt to install version 394.
  // The corrupted 500 should be skipped (and deleted at the end), so it
  // shouldn't block installation of 394 (anti-downgrade shouldn't trigger for
  // it).
  {
    base::FilePath src_file = GetTestCrxPath();
    ASSERT_TRUE(base::PathExists(src_file));

    EXPECT_EQ(INSTALL_COMPONENT_SUCCESS,
              InstallComponentForTesting(
                  src_file, installer_state, kTestComponents,
                  crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));

    // v500_corrupted should have been deleted because it lacked manifest.json.
    EXPECT_FALSE(base::PathExists(v500_corrupted));

    // The new version should be installed.
    base::FilePath v394 = component_root.AppendASCII("394");
    EXPECT_TRUE(base::PathExists(v394));
  }
}

TEST(InstallComponentTest, ArbitrarySourceFilePath) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  // Copy the test CRX into an arbitrary location with a non-standard directory
  // structure and file name.
  base::FilePath arbitrary_dir =
      temp_dir.GetPath().AppendASCII("some_random_cache_folder");
  ASSERT_TRUE(base::CreateDirectory(arbitrary_dir));
  base::FilePath arbitrary_source_file =
      arbitrary_dir.AppendASCII("unrelated_name.crx");
  ASSERT_TRUE(base::CopyFile(GetTestCrxPath(), arbitrary_source_file));

  // The component directory should be derived from the verified CRX ID rather
  // than the source file path, correctly installing under its CRX ID directory.
  EXPECT_EQ(INSTALL_COMPONENT_SUCCESS,
            InstallComponentForTesting(
                arbitrary_source_file, installer_state, kTestComponents,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));

  base::FilePath installed_version_dir =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId).AppendASCII("394");
  EXPECT_TRUE(base::PathExists(installed_version_dir));
  EXPECT_TRUE(
      base::PathExists(installed_version_dir.AppendASCII("manifest.json")));
}

TEST(InstallComponentTest, UserLevelInstallation) {
  InstallerState installer_state(InstallerState::USER_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  base::FilePath src_file = GetTestCrxPath();
  ASSERT_TRUE(base::PathExists(src_file));
  EXPECT_EQ(INSTALL_COMPONENT_SUCCESS,
            InstallComponentForTesting(
                src_file, installer_state, kTestComponents,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));
  base::FilePath target_dir =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId).AppendASCII("394");
  EXPECT_TRUE(base::PathExists(target_dir));
  EXPECT_TRUE(base::PathExists(target_dir.AppendASCII("manifest.json")));
}

}  // namespace installer
