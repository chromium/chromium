// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/copy_bundle_to_cache_command.h"

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::TestFuture;
using base::test::ValueIs;
using web_package::SignedWebBundleId;
using SessionType = IwaCacheClient::SessionType;

const SignedWebBundleId kBundleId = test::GetDefaultEd25519WebBundleId();
const web_package::test::Ed25519KeyPair kPublicKeyPair =
    test::GetDefaultEd25519KeyPair();

constexpr char kVersion1[] = "0.0.1";
constexpr char kVersion2[] = "2.0.0";

}  // namespace

// TODO(crbug.com/414793394): Reduce code duplications for cache command unit
// tests.
class CopyBundleToCacheCommandTest
    : public WebAppTest,
      public testing::WithParamInterface<SessionType> {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    ASSERT_TRUE(cache_root_dir_.CreateUniqueTempDir());
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, cache_root_dir_.GetPath());
  }

  base::FilePath CreateBundleInCacheDir(const SignedWebBundleId& bundle_id,
                                        const IwaVersion& version) {
    base::FilePath bundle_directory_path =
        GetBundleDirWithVersion(bundle_id, version);
    EXPECT_TRUE(base::CreateDirectory(bundle_directory_path));

    base::FilePath temp_file;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(CacheRootPath(), &temp_file));
    base::FilePath bundle_path =
        IwaCacheClient::GetBundleFullName(bundle_directory_path);
    EXPECT_TRUE(base::CopyFile(temp_file, bundle_path));
    return bundle_path;
  }

  base::FilePath GetBundleDirWithVersion(const SignedWebBundleId& bundle_id,
                                         const IwaVersion& version) {
    auto session_cache_dir =
        IwaCacheClient::GetCacheBaseDirectoryForSessionType(GetSessionType(),
                                                            CacheRootPath());
    return IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
        session_cache_dir, bundle_id, version);
  }

  base::FilePath GetBundleFullPath(const SignedWebBundleId& bundle_id,
                                   const IwaVersion& version) {
    return IwaCacheClient::GetBundleFullName(
        GetBundleDirWithVersion(bundle_id, version));
  }

  const base::FilePath& CacheRootPath() { return cache_root_dir_.GetPath(); }

  void ScheduleCommand(
      const web_package::SignedWebBundleId& web_bundle_id,
      base::OnceCallback<void(CopyBundleToCacheResult)> callback) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    fake_provider().scheduler().CopyIsolatedWebAppBundleToCache(
        url_info, GetSessionType(), std::move(callback));
  }

  void RestrictDirectoryPermission(const base::FilePath& dir) {
    // Allows to check that file exists, but disallows to change it.
    EXPECT_TRUE(
        SetPosixFilePermissions(dir, base::FILE_PERMISSION_EXECUTE_BY_USER));
  }

  std::unique_ptr<BundledIsolatedWebApp> CreateApp(const std::string& version) {
    base::FilePath bundle_path =
        IwaStorageOwnedBundle{"bundle-" + version, /*dev_mode=*/false}.GetPath(
            profile()->GetPath());
    EXPECT_TRUE(base::CreateDirectory(bundle_path.DirName()));

    std::unique_ptr<BundledIsolatedWebApp> app =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion(version))
            .BuildBundle(bundle_path, kPublicKeyPair);
    app->TrustSigningKey();
    app->FakeInstallPageState(profile());
    return app;
  }

  SessionType GetSessionType() { return GetParam(); }

 private:
  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_P(CopyBundleToCacheCommandTest, CopyBundleToCache) {
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());

  TestFuture<CopyBundleToCacheResult> copy_future;
  ScheduleCommand(kBundleId, copy_future.GetCallback());

  base::FilePath bundle_path =
      GetBundleFullPath(kBundleId, *IwaVersion::Create(kVersion1));
  EXPECT_THAT(copy_future.Get(),
              ValueIs(CopyBundleToCacheSuccess{bundle_path}));
  EXPECT_TRUE(base::PathExists(bundle_path));
}

TEST_P(CopyBundleToCacheCommandTest, AppNotInstalled) {
  TestFuture<CopyBundleToCacheResult> copy_future;
  ScheduleCommand(kBundleId, copy_future.GetCallback());

  EXPECT_THAT(copy_future.Get(),
              ErrorIs(CopyBundleToCacheError::kAppNotInstalled));
}

TEST_P(CopyBundleToCacheCommandTest, FailedToCreateDir) {
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());

  // Restricts cache root directory permissions, so copy to that directory will
  // fail.
  RestrictDirectoryPermission(CacheRootPath());

  TestFuture<CopyBundleToCacheResult> copy_future;
  ScheduleCommand(kBundleId, copy_future.GetCallback());

  EXPECT_THAT(copy_future.Get(),
              ErrorIs(CopyBundleToCacheError::kFailedToCreateDir));
}

TEST_P(CopyBundleToCacheCommandTest, FailedToCopyFile) {
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());

  // Bundle directory is already created, but restricted, so copy to that
  // directory will fail.
  base::FilePath bundle_directory_path =
      GetBundleDirWithVersion(kBundleId, *IwaVersion::Create(kVersion1));
  EXPECT_TRUE(base::CreateDirectory(bundle_directory_path));
  RestrictDirectoryPermission(bundle_directory_path);

  TestFuture<CopyBundleToCacheResult> copy_future;
  ScheduleCommand(kBundleId, copy_future.GetCallback());

  EXPECT_THAT(copy_future.Get(),
              ErrorIs(CopyBundleToCacheError::kFailedToCopyFile));
}

TEST_P(CopyBundleToCacheCommandTest, CopyBundleToCacheReplacesExistingFile) {
  base::FilePath existing_bundle =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());

  TestFuture<CopyBundleToCacheResult> copy_future;
  ScheduleCommand(kBundleId, copy_future.GetCallback());

  base::FilePath bundle_path =
      GetBundleFullPath(kBundleId, *IwaVersion::Create(kVersion1));
  EXPECT_THAT(copy_future.Get(),
              ValueIs(CopyBundleToCacheSuccess{bundle_path}));
  EXPECT_TRUE(base::PathExists(bundle_path));
}

TEST_P(CopyBundleToCacheCommandTest, CopyAnotherBundleVersion) {
  base::FilePath existing_bundle_path =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion2);
  ASSERT_THAT(app->Install(profile()), HasValue());

  TestFuture<CopyBundleToCacheResult> copy_future;
  ScheduleCommand(kBundleId, copy_future.GetCallback());

  base::FilePath updated_bundle_path =
      GetBundleFullPath(kBundleId, *IwaVersion::Create(kVersion2));
  EXPECT_THAT(copy_future.Get(),
              ValueIs(CopyBundleToCacheSuccess{updated_bundle_path}));
  // Check that both versions are cached.
  EXPECT_TRUE(base::PathExists(existing_bundle_path));
  EXPECT_TRUE(base::PathExists(updated_bundle_path));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CopyBundleToCacheCommandTest,
    testing::Values(IwaCacheClient::SessionType::kManagedGuestSession,
                    IwaCacheClient::SessionType::kKiosk));

}  // namespace web_app
