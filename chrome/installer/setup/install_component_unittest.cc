// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_component.h"

#include <windows.h>

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/access_token.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "chrome/installer/setup/component_interface.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crx_file/crx_creator.h"
#include "components/crx_file/crx_verifier.h"
#include "components/crx_file/id_util.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

#define FPL(x) FILE_PATH_LITERAL(x)

namespace installer {

namespace {

// The developer key ID of "valid_publisher.crx3".
constexpr std::string_view kTestDeveloperCrxId =
    "ojjgnpkioondelmggbekfhllhdaimnho";

// The SHA256 of the developer SubjectPublicKeyInfo of "valid_publisher.crx3".
constexpr std::array<uint8_t, crypto::hash::kSha256Size>
    kTestDeveloperPublicKeySHA256 = {
        0xe9, 0x96, 0xdf, 0xa8, 0xee, 0xd3, 0x4b, 0xc6, 0x61, 0x4a, 0x57,
        0xbb, 0x73, 0x08, 0xcd, 0x7e, 0x51, 0x9b, 0xcc, 0x69, 0x08, 0x41,
        0xe1, 0x96, 0x9f, 0x7c, 0xb1, 0x73, 0xef, 0x16, 0x80, 0x0a};

base::FilePath GetTestCrxPath(
    base::FilePath::StringViewType filename = FPL("valid_publisher.crx3")) {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  return test_data_root.Append(FPL("components"))
      .Append(FPL("test"))
      .Append(FPL("data"))
      .Append(FPL("crx_file"))
      .Append(filename);
}

// Creates a CRX signed with `signing_key` containing a dummy DLL and, if
// provided, a manifest.json with the contents `manifest_json`. `tag` is used to
// generate unique file names within `base_dir`.
base::FilePath CreateTestCrx(const base::FilePath& base_dir,
                             const crypto::keypair::PrivateKey& signing_key,
                             base::FilePath::StringViewType tag,
                             std::optional<std::string_view> manifest_json) {
  base::FilePath staging_dir =
      base_dir.Append(base::StrCat({FPL("staging_"), tag}));
  CHECK(base::CreateDirectory(staging_dir));

  if (manifest_json) {
    CHECK(base::WriteFile(staging_dir.Append(FPL("manifest.json")),
                          *manifest_json));
  }
  CHECK(base::WriteFile(staging_dir.Append(FPL("chrome_renderer.dll")),
                        "dummy_dll_payload"));

  base::FilePath zip_path =
      base_dir.Append(base::StrCat({FPL("patch_"), tag, FPL(".zip")}));
  CHECK(zip::Zip(staging_dir, zip_path, /*include_hidden_files=*/true));

  base::FilePath crx_path =
      base_dir.Append(base::StrCat({FPL("patch_"), tag, FPL(".crx3")}));
  CHECK_EQ(crx_file::CreatorResult::OK,
           crx_file::Create(crx_path, zip_path, signing_key));
  return crx_path;
}

// A component whose operations succeed or fail as directed. It is installed
// into a directory named for its CRX ID. It checks that, as ComponentInterface
// requires, its other methods are called only after ReadManifest succeeds.
class FakeComponent : public ComponentInterface {
 public:
  struct Results {
    bool read_manifest = true;
    // The status returned by DetermineDestinationRoot(), if it is to fail.
    std::optional<InstallStatus> determine_destination_root_error;
    bool initialize_destination_root = true;
  };

  // Constructs a component whose operations all succeed.
  FakeComponent() : FakeComponent(Results()) {}
  explicit FakeComponent(const Results& results) : results_(results) {}

  // Sets a DACL, in SDDL form, for InitializeDestinationRoot to apply to the
  // destination root.
  void set_destination_root_dacl(std::wstring dacl) {
    destination_root_dacl_ = std::move(dacl);
  }

  // ComponentInterface:
  bool ReadManifest(const base::DictValue& manifest) override {
    manifest_read_ = results_.read_manifest;
    return manifest_read_;
  }
  base::expected<base::FilePath, InstallStatus> DetermineDestinationRoot(
      const InstallerState& installer_state,
      std::string_view crx_id) override {
    EXPECT_TRUE(manifest_read_);
    if (results_.determine_destination_root_error) {
      return base::unexpected(*results_.determine_destination_root_error);
    }
    return installer_state.target_path().AppendASCII(crx_id);
  }
  bool InitializeDestinationRoot(
      const InstallerState& installer_state,
      const base::FilePath& destination_root) override {
    EXPECT_TRUE(manifest_read_);
    if (!results_.initialize_destination_root) {
      return false;
    }
    if (destination_root_dacl_.empty()) {
      return true;
    }
    std::optional<base::win::SecurityDescriptor> sd =
        base::win::SecurityDescriptor::FromSddl(destination_root_dacl_);
    return sd && sd->WriteToFile(destination_root, DACL_SECURITY_INFORMATION);
  }

 private:
  const Results results_;
  std::wstring destination_root_dacl_;
  bool manifest_read_ = false;
};

// Returns a component whose operations all succeed if `hash` is that of the
// test developer key, or nullptr otherwise.
std::unique_ptr<ComponentInterface> TestComponentFactory(
    const std::array<uint8_t, crypto::hash::kSha256Size>& hash) {
  if (hash == kTestDeveloperPublicKeySHA256) {
    return std::make_unique<FakeComponent>();
  }
  return nullptr;
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
  EXPECT_FALSE(GetComponentVersion(no_manifest_dir).IsValid());

  // Invalid version directory name with manifest.json.
  base::FilePath invalid_name_dir =
      temp_dir.GetPath().AppendASCII("not_a_version");
  ASSERT_TRUE(base::CreateDirectory(invalid_name_dir));
  ASSERT_TRUE(
      base::WriteFile(invalid_name_dir.AppendASCII("manifest.json"), "dummy"));
  EXPECT_FALSE(GetComponentVersion(invalid_name_dir).IsValid());

  // Non-canonical version directory name with manifest.json.
  base::FilePath non_canonical_dir = temp_dir.GetPath().AppendASCII("1.02.3.4");
  ASSERT_TRUE(base::CreateDirectory(non_canonical_dir));
  ASSERT_TRUE(
      base::WriteFile(non_canonical_dir.AppendASCII("manifest.json"), "dummy"));
  EXPECT_FALSE(GetComponentVersion(non_canonical_dir).IsValid());

  // Non-existent directory.
  base::FilePath non_existent_dir =
      temp_dir.GetPath().AppendASCII("non_existent");
  EXPECT_FALSE(GetComponentVersion(non_existent_dir).IsValid());
}

TEST(InstallComponentTest, FindHighestComponentVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath component_root = temp_dir.GetPath().AppendASCII("Component");

  // Non-existent root directory.
  EXPECT_FALSE(FindHighestComponentVersion(component_root).IsValid());

  ASSERT_TRUE(base::CreateDirectory(component_root));

  // Empty root directory.
  EXPECT_FALSE(FindHighestComponentVersion(component_root).IsValid());

  // Populate multiple version directories.
  base::FilePath v1 = component_root.AppendASCII("1.0.0.0");
  base::FilePath v2 = component_root.AppendASCII("2.0.0.0");
  base::FilePath v3_corrupted = component_root.AppendASCII("3.0.0.0");
  base::FilePath v4_non_canonical = component_root.AppendASCII("4.0.00.0");
  base::FilePath invalid_dir = component_root.AppendASCII("invalid_name");

  ASSERT_TRUE(base::CreateDirectory(v1));
  ASSERT_TRUE(base::WriteFile(v1.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(v2));
  ASSERT_TRUE(base::WriteFile(v2.AppendASCII("manifest.json"), "dummy"));

  // v3_corrupted has higher version name but lacks manifest.json.
  ASSERT_TRUE(base::CreateDirectory(v3_corrupted));

  // v4_non_canonical has a higher version and manifest.json, but its name is
  // not the canonical form of its version.
  ASSERT_TRUE(base::CreateDirectory(v4_non_canonical));
  ASSERT_TRUE(
      base::WriteFile(v4_non_canonical.AppendASCII("manifest.json"), "dummy"));

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
  base::FilePath v5_interrupted = component_root.AppendASCII("5.0.0.0");
  base::FilePath v6_non_canonical = component_root.AppendASCII("6.0.00.0");
  base::FilePath invalid_dir = component_root.AppendASCII("corrupted_dir");

  ASSERT_TRUE(base::CreateDirectory(v1));
  ASSERT_TRUE(base::WriteFile(v1.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(v2));
  ASSERT_TRUE(base::WriteFile(v2.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(v3));
  ASSERT_TRUE(base::WriteFile(v3.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(v4_corrupted));
  ASSERT_TRUE(base::WriteFile(v4_corrupted.AppendASCII("other.dll"), "dummy"));

  // An interrupted installation: the payload was copied in, but not the
  // manifest.
  ASSERT_TRUE(base::CreateDirectory(v5_interrupted));
  ASSERT_TRUE(
      base::WriteFile(v5_interrupted.AppendASCII("component.dll"), "dummy"));

  // A complete installation, but in a directory whose name is not the
  // canonical form of its version.
  ASSERT_TRUE(base::CreateDirectory(v6_non_canonical));
  ASSERT_TRUE(
      base::WriteFile(v6_non_canonical.AppendASCII("manifest.json"), "dummy"));

  ASSERT_TRUE(base::CreateDirectory(invalid_dir));

  ASSERT_TRUE(base::PathExists(v1));
  ASSERT_TRUE(base::PathExists(v2));
  ASSERT_TRUE(base::PathExists(v3));
  ASSERT_TRUE(base::PathExists(v4_corrupted));
  ASSERT_TRUE(base::PathExists(v5_interrupted));
  ASSERT_TRUE(base::PathExists(v6_non_canonical));
  ASSERT_TRUE(base::PathExists(invalid_dir));

  // Keep version 2.0.0.0.
  // v1 (< 2.0.0.0), v4_corrupted (missing manifest), v5_interrupted (missing
  // manifest), v6_non_canonical (non-canonical name), and invalid_dir should be
  // deleted. v2 (== 2.0.0.0) and v3 (> 2.0.0.0) should remain.
  DeleteInvalidComponentDirectories(component_root, base::Version("2.0.0.0"));

  EXPECT_FALSE(base::PathExists(v1));
  EXPECT_TRUE(base::PathExists(v2));
  EXPECT_TRUE(base::PathExists(v3));
  EXPECT_FALSE(base::PathExists(v4_corrupted));
  EXPECT_FALSE(base::PathExists(v5_interrupted));
  EXPECT_FALSE(base::PathExists(v6_non_canonical));
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
                  src_file, installer_state, &TestComponentFactory,
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
                src_file, installer_state, &TestComponentFactory,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));

  // Older versions should be deleted upon successful installation of 394.
  EXPECT_FALSE(base::PathExists(v100));
  EXPECT_FALSE(base::PathExists(v200));

  // 394 should exist.
  base::FilePath v394 = component_root.AppendASCII("394");
  EXPECT_TRUE(base::PathExists(v394));
}

TEST(InstallComponentTest, PublicKeyMismatch) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  base::FilePath src_file = GetTestCrxPath();
  ASSERT_TRUE(base::PathExists(src_file));

  // A component is supported only for a public key hash other than the CRX's.
  static constexpr std::array<uint8_t, crypto::hash::kSha256Size>
      kWrongPublicKeySHA256 = {0x12, 0x34};
  auto wrong_key_factory =
      [](const std::array<uint8_t, crypto::hash::kSha256Size>& hash)
      -> std::unique_ptr<ComponentInterface> {
    if (hash == kWrongPublicKeySHA256) {
      return std::make_unique<FakeComponent>();
    }
    return nullptr;
  };

  EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT,
            InstallComponentForTesting(
                src_file, installer_state, wrong_key_factory,
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
      GetTestCrxPath(FPL("valid_test_publisher.crx3"));
  ASSERT_TRUE(base::PathExists(test_publisher_crx));
  EXPECT_EQ(INSTALL_COMPONENT_FAILED_SIGNATURE,
            InstallComponentForTesting(
                test_publisher_crx, installer_state, &TestComponentFactory,
                crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF));

  // A CRX without publisher proof must fail when publisher proof is required.
  base::FilePath no_publisher_crx =
      GetTestCrxPath(FPL("valid_no_publisher.crx3"));
  ASSERT_TRUE(base::PathExists(no_publisher_crx));
  EXPECT_EQ(INSTALL_COMPONENT_FAILED_SIGNATURE,
            InstallComponentForTesting(
                no_publisher_crx, installer_state, &TestComponentFactory,
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
  const base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT,
            InstallComponent(src_file, installer_state, command_line));

  // Omitting the developer key from supported components must reject the CRX.
  auto empty_factory = [](const std::array<uint8_t, crypto::hash::kSha256Size>&)
      -> std::unique_ptr<ComponentInterface> { return nullptr; };
  EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT,
            InstallComponentForTesting(
                src_file, installer_state, empty_factory,
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
                  src_file, installer_state, &TestComponentFactory,
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
                arbitrary_source_file, installer_state, &TestComponentFactory,
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
                src_file, installer_state, &TestComponentFactory,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));
  base::FilePath target_dir =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId).AppendASCII("394");
  EXPECT_TRUE(base::PathExists(target_dir));
  EXPECT_TRUE(base::PathExists(target_dir.AppendASCII("manifest.json")));
}

TEST(InstallComponentTest, IncompletePreviousInstallationOverwritten) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  base::FilePath component_root =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId);
  ASSERT_TRUE(base::CreateDirectory(component_root));

  // Simulate an incomplete/interrupted installation of version 394 that left
  // behind part of its payload, but no manifest.json.
  base::FilePath v394_incomplete = component_root.AppendASCII("394");
  ASSERT_TRUE(base::CreateDirectory(v394_incomplete));
  ASSERT_TRUE(
      base::WriteFile(v394_incomplete.AppendASCII("stale.dll"), "dummy"));

  ASSERT_TRUE(base::PathExists(v394_incomplete));

  base::FilePath src_file = GetTestCrxPath();
  ASSERT_TRUE(base::PathExists(src_file));

  // The incomplete directory should not trigger anti-downgrade and should be
  // successfully replaced by the fresh installation.
  EXPECT_EQ(INSTALL_COMPONENT_SUCCESS,
            InstallComponentForTesting(
                src_file, installer_state, &TestComponentFactory,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));

  EXPECT_TRUE(base::PathExists(v394_incomplete));
  EXPECT_TRUE(base::PathExists(v394_incomplete.AppendASCII("manifest.json")));
  EXPECT_FALSE(base::PathExists(v394_incomplete.AppendASCII("stale.dll")));
}

// Verifies that the installed component's directories and files inherit their
// access rights from the destination root (e.g., as set by
// InitializeDestinationRoot) rather than retaining those of the temporary
// directory into which the component was unpacked.
TEST(InstallComponentTest, InstalledComponentInheritsDestinationRootAcl) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  // Give the destination root a protected DACL ("P") that grants only the
  // current user full control ("FA") of the root and the files ("OI") and
  // directories ("CI") within it. This differs from the DACL of the temporary
  // directory.
  ASSERT_OK_AND_ASSIGN(const auto token,
                       base::win::AccessToken::FromCurrentProcess());
  ASSERT_OK_AND_ASSIGN(const std::wstring user_sid,
                       token.User().ToSddlString());
  const std::wstring root_dacl =
      base::StrCat({L"D:P(A;OICI;FA;;;", user_sid, L")"});
  auto factory =
      [&root_dacl](const std::array<uint8_t, crypto::hash::kSha256Size>& hash)
      -> std::unique_ptr<ComponentInterface> {
    if (hash != kTestDeveloperPublicKeySHA256) {
      return nullptr;
    }
    auto component = std::make_unique<FakeComponent>();
    component->set_destination_root_dacl(root_dacl);
    return component;
  };
  ASSERT_EQ(INSTALL_COMPONENT_SUCCESS,
            InstallComponentForTesting(
                GetTestCrxPath(), installer_state, factory,
                crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));

  // Returns the DACL of `path` in SDDL form.
  auto get_dacl = [](const base::FilePath& path) {
    std::optional<base::win::SecurityDescriptor> sd =
        base::win::SecurityDescriptor::FromFile(path,
                                                DACL_SECURITY_INFORMATION);
    return sd ? sd->ToSddl(DACL_SECURITY_INFORMATION) : std::nullopt;
  };

  // The version directory and everything within it, including the manifest,
  // which is copied into place separately, have only the ACE that they inherit
  // ("ID") from the root. Directories pass it on to their own contents.
  const std::wstring expected_dir_dacl =
      base::StrCat({L"D:AI(A;OICIID;FA;;;", user_sid, L")"});
  const std::wstring expected_file_dacl =
      base::StrCat({L"D:AI(A;ID;FA;;;", user_sid, L")"});
  const base::FilePath version_dir =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId).AppendASCII("394");
  EXPECT_EQ(get_dacl(version_dir), expected_dir_dacl);
  base::FileEnumerator enumerator(
      version_dir, /*recursive=*/true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    SCOPED_TRACE(path);
    EXPECT_EQ(get_dacl(path), enumerator.GetInfo().IsDirectory()
                                  ? expected_dir_dacl
                                  : expected_file_dacl);
  }
}

TEST(InstallComponentTest, InvalidManifestRejected) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  const base::FilePath crx_dir = temp_dir.GetPath().Append(FPL("crx"));
  ASSERT_TRUE(base::CreateDirectory(crx_dir));

  auto signing_key = crypto::test::FixedRsa4096PrivateKeyForTesting();
  const std::array<uint8_t, crypto::hash::kSha256Size> public_key_sha256 =
      crypto::hash::Sha256(signing_key.ToSubjectPublicKeyInfo());
  const base::FilePath component_root = temp_dir.GetPath().AppendASCII(
      crx_file::id_util::GenerateIdFromHash(public_key_sha256));
  auto factory = [&public_key_sha256](
                     const std::array<uint8_t, crypto::hash::kSha256Size>& hash)
      -> std::unique_ptr<ComponentInterface> {
    return hash == public_key_sha256 ? std::make_unique<FakeComponent>()
                                     : nullptr;
  };

  static constexpr struct {
    base::FilePath::StringViewType tag;
    std::optional<std::string_view> manifest;
  } kCases[] = {
      {FPL("missing"), std::nullopt},
      {FPL("malformed"), R"({"name": "Test Component", )"},
      {FPL("not_a_dict"), R"(["Test Component"])"},
      {FPL("no_version"), R"({"name": "Test Component"})"},
      {FPL("invalid_version"),
       R"({"name": "Test Component", "version": "one"})"},
      {FPL("non_canonical_version"),
       R"({"name": "Test Component", "version": "1.02"})"},
      {FPL("non_string_version"),
       R"({"name": "Test Component", "version": 1})"},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.tag);
    base::FilePath crx_path =
        CreateTestCrx(crx_dir, signing_key, test_case.tag, test_case.manifest);
    EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT,
              InstallComponentForTesting(crx_path, installer_state, factory,
                                         crx_file::VerifierFormat::CRX3));
    EXPECT_FALSE(base::PathExists(component_root));
  }

  // A well-formed manifest succeeds under the same conditions.
  base::FilePath crx_path =
      CreateTestCrx(crx_dir, signing_key, FPL("valid"),
                    R"({"name": "Test Component", "version": "1.0"})");
  EXPECT_EQ(INSTALL_COMPONENT_SUCCESS,
            InstallComponentForTesting(crx_path, installer_state, factory,
                                       crx_file::VerifierFormat::CRX3));
  EXPECT_TRUE(base::PathExists(
      component_root.Append(FPL("1.0")).Append(FPL("manifest.json"))));
}

// Verifies that a failure partway through installation reverts the changes
// made to the target directory.
TEST(InstallComponentTest, FailedInstallationRollsBack) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());

  // Simulate an interrupted installation of version 394.
  const base::FilePath v394 =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId).AppendASCII("394");
  ASSERT_TRUE(base::CreateDirectory(v394));
  ASSERT_TRUE(base::WriteFile(v394.Append(FPL("stale.dll")), "stale"));
  ASSERT_TRUE(base::WriteFile(v394.Append(FPL("locked.dll")), "locked"));

  {
    // Hold a file in the leftover directory open without FILE_SHARE_DELETE so
    // that the directory can only be partially moved aside. Copying the new
    // payload into place then fails since the directory still exists.
    base::File locked(v394.Append(FPL("locked.dll")),
                      base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(locked.IsValid());
    EXPECT_EQ(INSTALL_COMPONENT_FAILED_INTERNAL,
              InstallComponentForTesting(
                  GetTestCrxPath(), installer_state, &TestComponentFactory,
                  crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF));
  }

  // The directory's original contents have been restored, and nothing from the
  // new payload remains.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(v394.Append(FPL("stale.dll")), &contents));
  EXPECT_EQ(contents, "stale");
  ASSERT_TRUE(
      base::ReadFileToString(v394.Append(FPL("locked.dll")), &contents));
  EXPECT_EQ(contents, "locked");
  int entry_count = 0;
  base::FileEnumerator(
      v394, /*recursive=*/true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES)
      .ForEach([&entry_count](const base::FilePath&) { ++entry_count; });
  EXPECT_EQ(entry_count, 2);
}

// Verifies that installation is aborted, without installing anything, when any
// of the component's operations fails.
TEST(InstallComponentTest, ComponentFailureAbortsInstallation) {
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  const base::FilePath target_dir =
      temp_dir.GetPath().AppendASCII(kTestDeveloperCrxId).AppendASCII("394");

  auto install = [&installer_state](const FakeComponent::Results& results) {
    return InstallComponentForTesting(
        GetTestCrxPath(), installer_state,
        [&results](const std::array<uint8_t, crypto::hash::kSha256Size>& hash)
            -> std::unique_ptr<ComponentInterface> {
          return hash == kTestDeveloperPublicKeySHA256
                     ? std::make_unique<FakeComponent>(results)
                     : nullptr;
        },
        crx_file::VerifierFormat::CRX3_WITH_TEST_PUBLISHER_PROOF);
  };

  EXPECT_EQ(INSTALL_COMPONENT_INVALID_INPUT, install({.read_manifest = false}));
  EXPECT_FALSE(base::PathExists(target_dir));

  // The component's status is returned if it cannot determine its root.
  for (const InstallStatus status :
       {INSTALL_COMPONENT_INVALID_INPUT, INSTALL_COMPONENT_FAILED_INTERNAL}) {
    SCOPED_TRACE(status);
    EXPECT_EQ(status, install({.determine_destination_root_error = status}));
    EXPECT_FALSE(base::PathExists(target_dir));
  }

  EXPECT_EQ(INSTALL_COMPONENT_FAILED_INTERNAL,
            install({.initialize_destination_root = false}));
  EXPECT_FALSE(base::PathExists(target_dir));

  // Installation succeeds when all operations succeed.
  EXPECT_EQ(INSTALL_COMPONENT_SUCCESS, install({}));
  EXPECT_TRUE(base::PathExists(target_dir.Append(FPL("manifest.json"))));
}

}  // namespace installer
