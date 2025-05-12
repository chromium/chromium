// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/get_bundle_cache_path_command.h"

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
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
const base::Version kVersion1 = base::Version("0.0.1");
const base::Version kVersion2 = base::Version("0.0.2");
const base::Version kVersion3 = base::Version("0.0.3");
const base::Version kVersion4 = base::Version("1.0.0");

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
                                        const base::Version& version) {
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
                                         const base::Version& version) {
    auto session_cache_dir =
        IwaCacheClient::GetCacheBaseDirectoryForSessionType(GetSessionType(),
                                                            CacheRootPath());
    return IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
        session_cache_dir, bundle_id, version);
  }

  void ScheduleCommand(
      const web_package::SignedWebBundleId& web_bundle_id,
      const std::optional<base::Version>& version,
      base::OnceCallback<void(GetBundleCachePathResult)> callback) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    fake_provider().scheduler().GetIsolatedWebAppBundleCachePath(
        url_info, version, GetSessionType(), std::move(callback));
  }

 private:
  const base::FilePath& CacheRootPath() { return cache_root_dir_.GetPath(); }

  SessionType GetSessionType() { return GetParam(); }

  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_P(GetBundleCachePathCommandTest, NoCachedPathToFetch) {
  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, /*version=*/std::nullopt,
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ErrorIs(GetBundleCachePathError::kIwaNotCached));
}

TEST_P(GetBundleCachePathCommandTest, RequiredVersionFound) {
  base::FilePath bundle_path = CreateBundleInCacheDir(kMainBundleId, kVersion1);

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, kVersion1, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path, kVersion1}));
}

TEST_P(GetBundleCachePathCommandTest, ProvidedVersionNotFound) {
  base::FilePath bundle_path = CreateBundleInCacheDir(kMainBundleId, kVersion1);

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, kVersion2, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ErrorIs(GetBundleCachePathError::kProvidedVersionNotFound));
}

TEST_P(GetBundleCachePathCommandTest, NoVersionProvided) {
  base::FilePath bundle_path = CreateBundleInCacheDir(kMainBundleId, kVersion1);

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, /*version=*/std::nullopt,
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path, kVersion1}));
}

TEST_P(GetBundleCachePathCommandTest, GetNewestVersionWhenVersionNotProvided) {
  base::FilePath bundle_path_v1 =
      CreateBundleInCacheDir(kMainBundleId, kVersion1);
  base::FilePath bundle_path_v3 =
      CreateBundleInCacheDir(kMainBundleId, kVersion3);
  base::FilePath bundle_path_v4 =
      CreateBundleInCacheDir(kMainBundleId, kVersion4);
  base::FilePath bundle_path_v2 =
      CreateBundleInCacheDir(kMainBundleId, kVersion2);

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, /*version=*/std::nullopt,
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path_v4, kVersion4}));
}

TEST_P(GetBundleCachePathCommandTest, GetCorrectVersion) {
  base::FilePath bundle_path_v2 =
      CreateBundleInCacheDir(kMainBundleId, kVersion2);
  base::FilePath bundle_path_v1 =
      CreateBundleInCacheDir(kMainBundleId, kVersion1);
  base::FilePath bundle_path_v3 =
      CreateBundleInCacheDir(kMainBundleId, kVersion3);

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, kVersion1, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path_v1, kVersion1}));
}

TEST_P(GetBundleCachePathCommandTest, GetCorrectIwa) {
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kMainBundleId, kVersion1);
  base::FilePath bundle_path2 = CreateBundleInCacheDir(kBundleId2, kVersion1);

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kBundleId2, kVersion1, get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ValueIs(GetBundleCachePathSuccess{bundle_path2, kVersion1}));
}

TEST_P(GetBundleCachePathCommandTest, IncorrectVersionParsed) {
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kMainBundleId, base::Version("aaaaa"));

  TestFuture<GetBundleCachePathResult> get_bundle_future;
  ScheduleCommand(kMainBundleId, /*version=*/std::nullopt,
                  get_bundle_future.GetCallback());

  EXPECT_THAT(get_bundle_future.Get(),
              ErrorIs(GetBundleCachePathError::kIwaNotCached));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GetBundleCachePathCommandTest,
    testing::Values(SessionType::kKiosk, SessionType::kManagedGuestSession));

}  // namespace web_app
