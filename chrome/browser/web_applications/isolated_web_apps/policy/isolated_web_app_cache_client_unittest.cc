// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using base::test::TestFuture;
using Bundle = IwaCacheClient::CachedBundleData;
using CopyBundleToCacheSuccess = IwaCacheClient::CopyBundleToCacheSuccess;
using CopyBundleToCacheError = IwaCacheClient::CopyBundleToCacheError;
using base::test::ErrorIs;
using base::test::ValueIs;
using testing::Field;
using web_package::SignedWebBundleId;

const SignedWebBundleId kBundleId = test::GetDefaultEd25519WebBundleId();
const base::Version kVersion1 = base::Version("0.0.1");
const base::Version kVersion2 = base::Version("0.0.2");
const base::Version kVersion3 = base::Version("0.0.3");
const base::Version kVersion4 = base::Version("1.0.0");

}  // namespace

enum SessionType {
  kMgs = 0,
  kKiosk = 1,
  kUser = 2,
};

class IwaCacheClientTest : public ::testing::TestWithParam<SessionType> {
 public:
  IwaCacheClientTest() = default;
  IwaCacheClientTest(const IwaCacheClientTest&) = delete;
  IwaCacheClientTest& operator=(const IwaCacheClientTest&) = delete;
  ~IwaCacheClientTest() override = default;

  void SetUp() override {
    user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(local_state_.Get()));

    switch (GetSessionType()) {
      case kMgs:
        test_managed_guest_session_ = std::make_unique<
            profiles::testing::ScopedTestManagedGuestSession>();
        break;
      case kKiosk:
        chromeos::SetUpFakeKioskSession();
        break;
      case kUser:
        NOTREACHED();
    }

    ASSERT_TRUE(cache_root_dir_.CreateUniqueTempDir());
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, cache_root_dir_.GetPath());

    // `IwaCacheClient` should be created after kiosk or MGS setup.
    cache_client_ = std::make_unique<IwaCacheClient>();
  }

  IwaCacheClient* cache_client() { return cache_client_.get(); }

  const base::FilePath& CacheDirPath() { return cache_root_dir_.GetPath(); }

  base::FilePath CreateBundleInCacheDir(const SignedWebBundleId& bundle_id,
                                        const base::Version& version) {
    base::FilePath bundle_directory_path =
        GetBundleDirWithVersion(bundle_id, version);
    EXPECT_TRUE(base::CreateDirectory(bundle_directory_path));

    base::FilePath temp_file;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(CacheDirPath(), &temp_file));

    base::FilePath bundle_path =
        bundle_directory_path.AppendASCII(kMainSwbnFileName);
    EXPECT_TRUE(base::CopyFile(temp_file, bundle_path));
    return bundle_path;
  }

  base::FilePath GetBundleDirWithVersion(const SignedWebBundleId& bundle_id,
                                         const base::Version& version) {
    base::FilePath bundle_directory_path = CacheDirPath();
    switch (GetSessionType()) {
      case SessionType::kMgs:
        bundle_directory_path =
            bundle_directory_path.AppendASCII(IwaCacheClient::kMgsDirName);
        break;
      case SessionType::kKiosk:
        bundle_directory_path =
            bundle_directory_path.AppendASCII(IwaCacheClient::kKioskDirName);
        break;
      case kUser:
        NOTREACHED() << "Caching is not supported in user session";
    }
    return bundle_directory_path.AppendASCII(bundle_id.id())
        .AppendASCII(version.GetString());
  }

  base::FilePath GetFullBundlePath(const SignedWebBundleId& bundle_id,
                                   const base::Version& version) {
    return GetBundleDirWithVersion(bundle_id, version)
        .AppendASCII(kMainSwbnFileName);
  }

  base::FilePath CreateFileInDir(base::ScopedTempDir& dir) {
    EXPECT_TRUE(dir.CreateUniqueTempDir());
    base::FilePath temp_file;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(dir.GetPath(), &temp_file));
    return temp_file;
  }

  void ResetCacheDir() {
    cache_root_dir_override_.reset();
    cache_client()->SetCacheDirForTesting(
        base::PathService::CheckedGet(ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE));
  }

 private:
  SessionType GetSessionType() { return GetParam(); }

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebAppBundleCache};
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::ScopedUserManager user_manager_;
  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
  std::unique_ptr<IwaCacheClient> cache_client_;

  // This is set only for MGS session.
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
};

TEST_P(IwaCacheClientTest, NoCachedPathToFetch) {
  TestFuture<std::optional<Bundle>> bundle_future;
  cache_client()->GetCacheFilePath(kBundleId,
                                   /*version=*/std::nullopt,
                                   bundle_future.GetCallback());

  EXPECT_FALSE(bundle_future.Get());
}

TEST_P(IwaCacheClientTest, GetCachedPathWithRequiredVersion) {
  base::FilePath bundle_path = CreateBundleInCacheDir(kBundleId, kVersion1);

  TestFuture<std::optional<Bundle>> bundle_future;
  cache_client()->GetCacheFilePath(kBundleId, kVersion1,
                                   bundle_future.GetCallback());

  EXPECT_EQ(bundle_future.Get()->path, bundle_path);
  EXPECT_EQ(bundle_future.Get()->version, kVersion1);
}

TEST_P(IwaCacheClientTest, NoCachedPathWhenVersionNotCached) {
  base::FilePath bundle_path = CreateBundleInCacheDir(kBundleId, kVersion1);

  TestFuture<std::optional<Bundle>> bundle_future;
  cache_client()->GetCacheFilePath(kBundleId, kVersion2,
                                   bundle_future.GetCallback());

  EXPECT_FALSE(bundle_future.Get());
}

TEST_P(IwaCacheClientTest, GetCachedPathNoVersionProvided) {
  base::FilePath bundle_path = CreateBundleInCacheDir(kBundleId, kVersion1);

  TestFuture<std::optional<Bundle>> bundle_future;
  cache_client()->GetCacheFilePath(kBundleId, /*version=*/std::nullopt,
                                   bundle_future.GetCallback());

  EXPECT_EQ(bundle_future.Get()->path, bundle_path);
  EXPECT_EQ(bundle_future.Get()->version, kVersion1);
}

TEST_P(IwaCacheClientTest, GetNewestVersionWhenVersionNotProvided) {
  base::FilePath bundle_path_v1 = CreateBundleInCacheDir(kBundleId, kVersion1);
  base::FilePath bundle_path_v3 = CreateBundleInCacheDir(kBundleId, kVersion3);
  base::FilePath bundle_path_v2 = CreateBundleInCacheDir(kBundleId, kVersion2);
  base::FilePath bundle_path_v4 = CreateBundleInCacheDir(kBundleId, kVersion4);

  TestFuture<std::optional<Bundle>> bundle_future;
  cache_client()->GetCacheFilePath(kBundleId, /*version=*/std::nullopt,
                                   bundle_future.GetCallback());

  EXPECT_EQ(bundle_future.Get()->path, bundle_path_v4);
  EXPECT_EQ(bundle_future.Get()->version, kVersion4);
}

TEST_P(IwaCacheClientTest, GetCorrectVersion) {
  base::FilePath bundle_path_v2 = CreateBundleInCacheDir(kBundleId, kVersion2);
  base::FilePath bundle_path_v1 = CreateBundleInCacheDir(kBundleId, kVersion1);
  base::FilePath bundle_path_v3 = CreateBundleInCacheDir(kBundleId, kVersion3);

  TestFuture<std::optional<Bundle>> bundle_future;
  cache_client()->GetCacheFilePath(kBundleId, kVersion1,
                                   bundle_future.GetCallback());

  EXPECT_EQ(bundle_future.Get()->path, bundle_path_v1);
  EXPECT_EQ(bundle_future.Get()->version, kVersion1);
}

TEST_P(IwaCacheClientTest, GetCorrectBundle) {
  SignedWebBundleId web_bundle_id2 = test::GetDefaultEcdsaP256WebBundleId();

  base::FilePath bundle_path1 = CreateBundleInCacheDir(kBundleId, kVersion1);
  base::FilePath bundle_path2 =
      CreateBundleInCacheDir(web_bundle_id2, kVersion1);

  TestFuture<std::optional<Bundle>> bundle_future;
  cache_client()->GetCacheFilePath(web_bundle_id2, kVersion1,
                                   bundle_future.GetCallback());

  EXPECT_EQ(bundle_future.Get()->path, bundle_path2);
  EXPECT_EQ(bundle_future.Get()->version, kVersion1);
}

TEST_P(IwaCacheClientTest, IncorrectVersionParsed) {
  base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kBundleId, base::Version("aaaaa"));

  TestFuture<std::optional<Bundle>> bundle_future;
  cache_client()->GetCacheFilePath(kBundleId, /*version=*/std::nullopt,
                                   bundle_future.GetCallback());

  EXPECT_FALSE(bundle_future.Get());
}

TEST_P(IwaCacheClientTest, CopyBundleToCache) {
  base::ScopedTempDir original_file_dir;
  base::FilePath original_file = CreateFileInDir(original_file_dir);

  TestFuture<base::expected<CopyBundleToCacheSuccess, CopyBundleToCacheError>>
      copy_future;
  cache_client()->CopyBundleToCache(original_file, kBundleId, kVersion1,
                                    copy_future.GetCallback());

  EXPECT_THAT(copy_future.Get(),
              ValueIs(Field(&CopyBundleToCacheSuccess::cached_bundle_path,
                            GetFullBundlePath(kBundleId, kVersion1))));
}

TEST_P(IwaCacheClientTest, FailedToCreateDirForFileCopying) {
  // Reset the cache directory back to the production value, since tests cannot
  // create subdirectories there.
  ResetCacheDir();

  base::ScopedTempDir original_file_dir;
  base::FilePath original_file = CreateFileInDir(original_file_dir);

  TestFuture<base::expected<CopyBundleToCacheSuccess, CopyBundleToCacheError>>
      copy_future;
  cache_client()->CopyBundleToCache(original_file, kBundleId, kVersion1,
                                    copy_future.GetCallback());

  EXPECT_THAT(copy_future.Get(),
              ErrorIs(CopyBundleToCacheError::kFailedToCreateDir));
}

TEST_P(IwaCacheClientTest, FailedToCopyFile) {
  TestFuture<base::expected<CopyBundleToCacheSuccess, CopyBundleToCacheError>>
      copy_future;
  cache_client()->CopyBundleToCache(base::FilePath("/do/not/exist/file"),
                                    kBundleId, kVersion1,
                                    copy_future.GetCallback());

  EXPECT_THAT(copy_future.Get(),
              ErrorIs(CopyBundleToCacheError::kFailedToCopyFile));
}

TEST_P(IwaCacheClientTest, CopyAndGet) {
  base::ScopedTempDir original_file_dir;
  base::FilePath original_file = CreateFileInDir(original_file_dir);
  TestFuture<base::expected<CopyBundleToCacheSuccess, CopyBundleToCacheError>>
      copy_future;

  cache_client()->CopyBundleToCache(original_file, kBundleId, kVersion1,
                                    copy_future.GetCallback());
  EXPECT_TRUE(copy_future.Get().has_value());

  TestFuture<std::optional<Bundle>> get_future;
  cache_client()->GetCacheFilePath(kBundleId, kVersion1,
                                   get_future.GetCallback());

  EXPECT_EQ(get_future.Get()->path, GetFullBundlePath(kBundleId, kVersion1));
  EXPECT_EQ(get_future.Get()->version, kVersion1);
}

TEST_P(IwaCacheClientTest, CopyBundleToCacheReplacesExistingFile) {
  base::ScopedTempDir original_file_dir;
  base::FilePath original_file = CreateFileInDir(original_file_dir);

  base::FilePath existing_bundle = CreateBundleInCacheDir(kBundleId, kVersion1);

  TestFuture<base::expected<CopyBundleToCacheSuccess, CopyBundleToCacheError>>
      copy_future;
  cache_client()->CopyBundleToCache(original_file, kBundleId, kVersion1,
                                    copy_future.GetCallback());

  EXPECT_THAT(copy_future.Get(),
              ValueIs(Field(&CopyBundleToCacheSuccess::cached_bundle_path,
                            GetFullBundlePath(kBundleId, kVersion1))));
}

TEST_P(IwaCacheClientTest, CopyAnotherBundleVersion) {
  base::ScopedTempDir original_file_dir;
  base::FilePath original_file = CreateFileInDir(original_file_dir);

  base::FilePath existing_bundle_path =
      CreateBundleInCacheDir(kBundleId, kVersion1);

  TestFuture<base::expected<CopyBundleToCacheSuccess, CopyBundleToCacheError>>
      copy_future;
  cache_client()->CopyBundleToCache(original_file, kBundleId, kVersion2,
                                    copy_future.GetCallback());

  EXPECT_THAT(copy_future.Get(),
              ValueIs(Field(&CopyBundleToCacheSuccess::cached_bundle_path,
                            GetFullBundlePath(kBundleId, kVersion2))));

  // Check that both versions are cached.
  base::PathExists(existing_bundle_path);
  base::PathExists(GetFullBundlePath(kBundleId, kVersion2));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheClientTest,
    testing::Values(kMgs, kKiosk));

struct IwaCacheClientDeathTestParam {
  SessionType session_type;
  bool feature_enabled;
  bool should_crash;
};

class IwaCacheClientDeathTest
    : public ::testing::TestWithParam<IwaCacheClientDeathTestParam> {
 public:
  IwaCacheClientDeathTest() {
    if (GetParam().feature_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          {features::kIsolatedWebAppBundleCache});
    }
  }
  IwaCacheClientDeathTest(const IwaCacheClientDeathTest&) = delete;
  IwaCacheClientDeathTest& operator=(const IwaCacheClientDeathTest&) = delete;
  ~IwaCacheClientDeathTest() override = default;

  void SetUp() override {
    user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(local_state_.Get()));

    switch (GetParam().session_type) {
      case kMgs:
        test_managed_guest_session_ = std::make_unique<
            profiles::testing::ScopedTestManagedGuestSession>();
        break;
      case kKiosk:
        chromeos::SetUpFakeKioskSession();
        break;
      case kUser:
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  user_manager::ScopedUserManager user_manager_;

  // This is set only for MGS session.
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
};

TEST_P(IwaCacheClientDeathTest, CreateClient) {
  if (GetParam().should_crash) {
    EXPECT_DEATH(IwaCacheClient(), "");
  } else {
    IwaCacheClient();
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheClientDeathTest,
    testing::ValuesIn(std::vector<IwaCacheClientDeathTestParam>{
        {
            .session_type = kMgs,
            .feature_enabled = true,
            .should_crash = false,
        },
        {
            .session_type = kKiosk,
            .feature_enabled = true,
            .should_crash = false,
        },
        {
            .session_type = kMgs,
            .feature_enabled = false,
            .should_crash = true,
        },
        {
            .session_type = kKiosk,
            .feature_enabled = false,
            .should_crash = true,
        },
        {
            .session_type = kUser,
            .feature_enabled = true,
            .should_crash = true,
        },
        {
            .session_type = kUser,
            .feature_enabled = false,
            .should_crash = true,
        },
    }));

}  // namespace web_app
