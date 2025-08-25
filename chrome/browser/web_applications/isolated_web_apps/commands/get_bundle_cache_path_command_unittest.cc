// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/get_bundle_cache_path_command.h"

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
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
using base::test::TestFuture;
using base::test::ValueIs;
using web_package::SignedWebBundleId;
using SessionType = IwaCacheClient::SessionType;

const SignedWebBundleId kMainBundleId = test::GetDefaultEd25519WebBundleId();
const web_package::test::Ed25519KeyPair kPublicKeyPair =
    test::GetDefaultEd25519KeyPair();
SignedWebBundleId kBundleId2 = test::GetDefaultEcdsaP256WebBundleId();

constexpr char kGetBundleCachePathSuccessMetric[] =
    "WebApp.Isolated.GetBundleCachePathSuccess";
constexpr char kGetBundleCachePathErrorMetric[] =
    "WebApp.Isolated.GetBundleCachePathError";

}  // namespace

class GetBundleCachePathCommandTest
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

  void ScheduleCommand(
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::optional<IwaVersion>& version,
      base::OnceCallback<void(GetBundleCachePathResult)> callback) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    fake_provider().scheduler().GetIsolatedWebAppBundleCachePath(
        url_info, version, GetSessionType(), std::move(callback));
  }

  void ExpectEmptyGetBundleCachePathMetrics() {
    histogram_tester_.ExpectTotalCount(kGetBundleCachePathSuccessMetric, 0);
    histogram_tester_.ExpectTotalCount(kGetBundleCachePathErrorMetric, 0);
  }

  void ExpectSuccessGetBundleCachePathMetric() {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(kGetBundleCachePathSuccessMetric),
        BucketsAre(base::Bucket(true, 1)));
    histogram_tester_.ExpectTotalCount(kGetBundleCachePathErrorMetric, 0);
  }

  void ExpectErrorGetBundleCachePathMetric(
      const GetBundleCachePathError& error) {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(kGetBundleCachePathSuccessMetric),
        BucketsAre(base::Bucket(false, 1)));
    EXPECT_THAT(histogram_tester_.GetAllSamples(kGetBundleCachePathErrorMetric),
                BucketsAre(base::Bucket(error, 1)));
  }

 private:
  const base::FilePath& CacheRootPath() { return cache_root_dir_.GetPath(); }

  SessionType GetSessionType() { return GetParam(); }

  base::HistogramTester histogram_tester_;
  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_P(GetBundleCachePathCommandTest, NoCachedPathToFetch) {
  ExpectEmptyGetBundleCachePathMetrics();
  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, /*version=*/std::nullopt,
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ErrorIs(GetBundleCachePathError::kIwaNotCached));
  ExpectErrorGetBundleCachePathMetric(GetBundleCachePathError::kIwaNotCached);
}

TEST_P(GetBundleCachePathCommandTest, RequiredVersionFound) {
  ExpectEmptyGetBundleCachePathMetrics();
  base::FilePath bundle_path =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.1"));

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, *IwaVersion::Create("0.0.1"),
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path,
                                                *IwaVersion::Create("0.0.1")}));
  ExpectSuccessGetBundleCachePathMetric();
}

TEST_P(GetBundleCachePathCommandTest, ProvidedVersionNotFound) {
  ExpectEmptyGetBundleCachePathMetrics();
  base::FilePath bundle_path =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.1"));

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, *IwaVersion::Create("0.0.2"),
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ErrorIs(GetBundleCachePathError::kProvidedVersionNotFound));
  ExpectErrorGetBundleCachePathMetric(
      GetBundleCachePathError::kProvidedVersionNotFound);
}

TEST_P(GetBundleCachePathCommandTest, NoVersionProvided) {
  base::FilePath bundle_path =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.1"));

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, /*version=*/std::nullopt,
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path,
                                                *IwaVersion::Create("0.0.1")}));
}

TEST_P(GetBundleCachePathCommandTest, GetNewestVersionWhenVersionNotProvided) {
  base::FilePath bundle_path_v1 =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.1"));
  base::FilePath bundle_path_v3 =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.3"));
  base::FilePath bundle_path_v4 =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("1.0.0"));
  base::FilePath bundle_path_v2 =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.2"));

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, /*version=*/std::nullopt,
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path_v4,
                                                *IwaVersion::Create("1.0.0")}));
}

TEST_P(GetBundleCachePathCommandTest, GetCorrectVersion) {
  base::FilePath bundle_path_v2 =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.2"));
  base::FilePath bundle_path_v1 =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.1"));
  base::FilePath bundle_path_v3 =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.3"));

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, *IwaVersion::Create("0.0.1"),
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path_v1,
                                                *IwaVersion::Create("0.0.1")}));
}

TEST_P(GetBundleCachePathCommandTest, GetCorrectIwa) {
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kMainBundleId, *IwaVersion::Create("0.0.1"));
  base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId2, *IwaVersion::Create("0.0.1"));

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kBundleId2, *IwaVersion::Create("0.0.1"),
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path2,
                                                *IwaVersion::Create("0.0.1")}));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GetBundleCachePathCommandTest,
    testing::Values(SessionType::kKiosk, SessionType::kManagedGuestSession));

}  // namespace web_app
