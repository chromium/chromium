// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/remove_obsolete_bundle_versions_cache_command.h"

#include <string>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/get_bundle_cache_path_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
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
constexpr char kVersion2[] = "0.0.2";
constexpr char kVersion3[] = "0.0.3";

constexpr char kRemoveObsoleteBundleVersionsSuccessMetric[] =
    "WebApp.Isolated.RemoveObsoleteBundleVersionsSuccess";
constexpr char kRemoveObsoleteBundleVersionsErrorMetric[] =
    "WebApp.Isolated.RemoveObsoleteBundleVersionsError";

}  // namespace

class RemoveObsoleteBundleVersionsCacheCommandTest
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
    auto session_cache_dir = GetSessionCacheDir();
    return IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
        session_cache_dir, bundle_id, version);
  }

  void ScheduleCommand(
      const web_package::SignedWebBundleId& web_bundle_id,
      base::OnceCallback<void(RemoveObsoleteBundleVersionsResult)> callback) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    fake_provider().scheduler().RemoveObsoleteIsolatedWebAppVersionsCache(
        url_info, GetSessionType(), std::move(callback));
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

  bool RestrictDirectoryPermission(const base::FilePath& dir) {
    // Allows to check that file exists, but disallows to change it.
    return SetPosixFilePermissions(dir, base::FILE_PERMISSION_EXECUTE_BY_USER |
                                            base::FILE_PERMISSION_READ_BY_USER);
  }

  void ExpectEmptyRemoveObsoleteBundleVersionsMetrics() {
    histogram_tester_.ExpectTotalCount(
        kRemoveObsoleteBundleVersionsSuccessMetric, 0);
    histogram_tester_.ExpectTotalCount(kRemoveObsoleteBundleVersionsErrorMetric,
                                       0);
  }

  void ExpectSuccessRemoveObsoleteBundleVersionsMetric() {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    kRemoveObsoleteBundleVersionsSuccessMetric),
                BucketsAre(base::Bucket(true, 1)));
    histogram_tester_.ExpectTotalCount(kRemoveObsoleteBundleVersionsErrorMetric,
                                       0);
  }

  void ExpectErrorRemoveObsoleteBundleVersionsMetric(
      const RemoveObsoleteBundleVersionsError::Type& error) {
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    kRemoveObsoleteBundleVersionsSuccessMetric),
                BucketsAre(base::Bucket(false, 1)));
    EXPECT_THAT(histogram_tester_.GetAllSamples(
                    kRemoveObsoleteBundleVersionsErrorMetric),
                BucketsAre(base::Bucket(error, 1)));
  }

 private:
  const base::FilePath& CacheRootPath() { return cache_root_dir_.GetPath(); }

  base::FilePath GetSessionCacheDir() {
    return IwaCacheClient::GetCacheBaseDirectoryForSessionType(GetSessionType(),
                                                               CacheRootPath());
  }

  SessionType GetSessionType() { return GetParam(); }

  base::HistogramTester histogram_tester_;
  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest, AppNotInstalled) {
  ExpectEmptyRemoveObsoleteBundleVersionsMetrics();
  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ErrorIs(RemoveObsoleteBundleVersionsError(
                  RemoveObsoleteBundleVersionsError::Type::kAppNotInstalled)));
  ExpectErrorRemoveObsoleteBundleVersionsMetric(
      RemoveObsoleteBundleVersionsError::Type::kAppNotInstalled);
}

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest,
       InstalledVersionNotCached) {
  ExpectEmptyRemoveObsoleteBundleVersionsMetrics();
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());

  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ErrorIs(RemoveObsoleteBundleVersionsError(
                  RemoveObsoleteBundleVersionsError::Type::
                      kInstalledVersionNotCached)));
  ExpectErrorRemoveObsoleteBundleVersionsMetric(
      RemoveObsoleteBundleVersionsError::Type::kInstalledVersionNotCached);
}

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest,
       OnlyInstalledVersionIsCached) {
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());
  base::FilePath bundle_path =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));

  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(RemoveObsoleteBundleVersionsSuccess(0)));
  EXPECT_TRUE(base::PathExists(bundle_path));
}

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest, RemoveOldVersion) {
  ExpectEmptyRemoveObsoleteBundleVersionsMetrics();
  // `kVersion2` is installed, remove `kVersion1`.
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion2);
  ASSERT_THAT(app->Install(profile()), HasValue());
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));
  base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion2));

  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(RemoveObsoleteBundleVersionsSuccess(1)));
  EXPECT_FALSE(base::PathExists(bundle_path1));
  EXPECT_TRUE(base::PathExists(bundle_path2));
  ExpectSuccessRemoveObsoleteBundleVersionsMetric();
}

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest, RemoveNewVersion) {
  // `kVersion1` is installed, remove `kVersion2`.
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));
  base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion2));

  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(RemoveObsoleteBundleVersionsSuccess(1)));
  EXPECT_TRUE(base::PathExists(bundle_path1));
  EXPECT_FALSE(base::PathExists(bundle_path2));
}

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest, RemoveTwoVersions) {
  // `kVersion2` is installed, remove `kVersion1` and `kVersion3`.
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion2);
  ASSERT_THAT(app->Install(profile()), HasValue());
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));
  base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion2));
  base::FilePath bundle_path3 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion3));

  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(RemoveObsoleteBundleVersionsSuccess(2)));
  EXPECT_FALSE(base::PathExists(bundle_path1));
  EXPECT_TRUE(base::PathExists(bundle_path2));
  EXPECT_FALSE(base::PathExists(bundle_path3));
}

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest, AppNotInstalledButCached) {
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));

  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ErrorIs(RemoveObsoleteBundleVersionsError(
                  RemoveObsoleteBundleVersionsError::Type::kAppNotInstalled)));
  EXPECT_TRUE(base::PathExists(bundle_path1));
}

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest,
       CouldNotDeleteAllVersions) {
  // `kVersion1` is installed, cannot remove `kVersion2` and `kVersion3`
  // because of restricted permissions.
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));
  base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion2));
  base::FilePath bundle_path3 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion3));
  EXPECT_TRUE(RestrictDirectoryPermission(
      GetBundleDirWithVersion(kBundleId, *IwaVersion::Create(kVersion2))));
  EXPECT_TRUE(RestrictDirectoryPermission(
      GetBundleDirWithVersion(kBundleId, *IwaVersion::Create(kVersion3))));

  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(
      get_bundle_future.Get(),
      ErrorIs(RemoveObsoleteBundleVersionsError(
          RemoveObsoleteBundleVersionsError::Type::kCouldNotDeleteAllVersions,
          2)));
  EXPECT_TRUE(base::PathExists(bundle_path1));
  EXPECT_TRUE(base::PathExists(bundle_path2));
  EXPECT_TRUE(base::PathExists(bundle_path3));
}

TEST_P(RemoveObsoleteBundleVersionsCacheCommandTest, CouldNotDeleteOneVersion) {
  // `kVersion1` is installed, cannot remove `kVersion2` because of restricted
  // permissions, but `kVersion3` is deleted.
  std::unique_ptr<BundledIsolatedWebApp> app = CreateApp(kVersion1);
  ASSERT_THAT(app->Install(profile()), HasValue());
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion1));
  base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion2));
  base::FilePath bundle_path3 =
      CreateBundleInCacheDir(kBundleId, *IwaVersion::Create(kVersion3));
  EXPECT_TRUE(RestrictDirectoryPermission(
      GetBundleDirWithVersion(kBundleId, *IwaVersion::Create(kVersion2))));

  TestFuture<RemoveObsoleteBundleVersionsResult> get_bundle_future;
  ScheduleCommand(kBundleId, get_bundle_future.GetCallback());

  EXPECT_THAT(
      get_bundle_future.Get(),
      ErrorIs(RemoveObsoleteBundleVersionsError(
          RemoveObsoleteBundleVersionsError::Type::kCouldNotDeleteAllVersions,
          1)));
  EXPECT_TRUE(base::PathExists(bundle_path1));
  EXPECT_TRUE(base::PathExists(bundle_path2));
  EXPECT_FALSE(base::PathExists(bundle_path3));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    RemoveObsoleteBundleVersionsCacheCommandTest,
    testing::Values(SessionType::kKiosk, SessionType::kManagedGuestSession));

}  // namespace web_app
