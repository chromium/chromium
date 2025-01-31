// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
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

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::DirectoryExists(CacheDirPath()));

    cache_client_ = std::make_unique<IwaCacheClient>();
    cache_client_->SetCacheDirForTesting(CacheDirPath());
  }

  IwaCacheClient* cache_client() { return cache_client_.get(); }

  const web_package::SignedWebBundleId& web_bundle_id() {
    return web_bundle_id_;
  }

  const base::FilePath& CacheDirPath() { return temp_dir_.GetPath(); }

  base::FilePath CreateBundleFile(
      const web_package::SignedWebBundleId& bundle_id,
      const base::Version& version) {
    base::FilePath bundle_directory_path = CacheDirPath();
    if (GetSessionType() == kMgs) {
      bundle_directory_path =
          bundle_directory_path.AppendASCII(IwaCacheClient::kMgsDirName);
    } else if (GetSessionType() == kKiosk) {
      bundle_directory_path =
          bundle_directory_path.AppendASCII(IwaCacheClient::kKioskDirName);
    }
    bundle_directory_path = bundle_directory_path.AppendASCII(bundle_id.id())
                                .AppendASCII(version.GetString());

    EXPECT_TRUE(base::CreateDirectory(bundle_directory_path));

    base::FilePath temp_file;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(CacheDirPath(), &temp_file));

    base::FilePath bundle_path =
        bundle_directory_path.AppendASCII(kMainSwbnFileName);
    EXPECT_TRUE(base::CopyFile(temp_file, bundle_path));
    return bundle_path;
  }

 private:
  SessionType GetSessionType() { return GetParam(); }

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebAppBundleCache};

  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::ScopedUserManager user_manager_;

  // This is set only for MGS session.
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;

  web_package::SignedWebBundleId web_bundle_id_ =
      test::GetDefaultEd25519WebBundleId();
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<IwaCacheClient> cache_client_;
};

TEST_P(IwaCacheClientTest, NoCachedPath) {
  TestFuture<std::optional<base::FilePath>> file_path_future;
  cache_client()->GetCacheFilePath(web_bundle_id(),
                                   /*version=*/std::nullopt,
                                   file_path_future.GetCallback());

  ASSERT_FALSE(file_path_future.Get());
}

TEST_P(IwaCacheClientTest, HasCachedPathWithRequiredVersion) {
  base::FilePath bundle_path = CreateBundleFile(web_bundle_id(), kVersion1);

  TestFuture<std::optional<base::FilePath>> file_path_future;
  cache_client()->GetCacheFilePath(web_bundle_id(), kVersion1,
                                   file_path_future.GetCallback());

  EXPECT_EQ(file_path_future.Get(), bundle_path);
}

TEST_P(IwaCacheClientTest, NoCachedPathWhenVersionNotCached) {
  base::FilePath bundle_path = CreateBundleFile(web_bundle_id(), kVersion1);

  TestFuture<std::optional<base::FilePath>> file_path_future;
  cache_client()->GetCacheFilePath(web_bundle_id(), kVersion2,
                                   file_path_future.GetCallback());

  ASSERT_FALSE(file_path_future.Get());
}

TEST_P(IwaCacheClientTest, HasCachedPathNoVersionProvided) {
  base::FilePath bundle_path = CreateBundleFile(web_bundle_id(), kVersion1);

  TestFuture<std::optional<base::FilePath>> file_path_future;
  cache_client()->GetCacheFilePath(web_bundle_id(), /*version=*/std::nullopt,
                                   file_path_future.GetCallback());

  EXPECT_EQ(file_path_future.Get(), bundle_path);
}

TEST_P(IwaCacheClientTest, GetNewestVersionWhenVersionNotProvided) {
  base::FilePath bundle_path_v1 = CreateBundleFile(web_bundle_id(), kVersion1);
  base::FilePath bundle_path_v3 = CreateBundleFile(web_bundle_id(), kVersion3);
  base::FilePath bundle_path_v2 = CreateBundleFile(web_bundle_id(), kVersion2);
  base::FilePath bundle_path_v4 = CreateBundleFile(web_bundle_id(), kVersion4);

  TestFuture<std::optional<base::FilePath>> file_path_future;
  cache_client()->GetCacheFilePath(web_bundle_id(), /*version=*/std::nullopt,
                                   file_path_future.GetCallback());

  EXPECT_EQ(file_path_future.Get(), bundle_path_v4);
}

TEST_P(IwaCacheClientTest, ReturnCorrectVersion) {
  base::FilePath bundle_path_v2 = CreateBundleFile(web_bundle_id(), kVersion2);
  base::FilePath bundle_path_v1 = CreateBundleFile(web_bundle_id(), kVersion1);
  base::FilePath bundle_path_v3 = CreateBundleFile(web_bundle_id(), kVersion3);

  TestFuture<std::optional<base::FilePath>> file_path_future;
  cache_client()->GetCacheFilePath(web_bundle_id(), kVersion1,
                                   file_path_future.GetCallback());

  EXPECT_EQ(file_path_future.Get(), bundle_path_v1);
}

TEST_P(IwaCacheClientTest, ReturnCorrectBundle) {
  web_package::SignedWebBundleId web_bundle_id2 =
      test::GetDefaultEcdsaP256WebBundleId();

  base::FilePath bundle_path1 = CreateBundleFile(web_bundle_id(), kVersion1);
  base::FilePath bundle_path2 = CreateBundleFile(web_bundle_id2, kVersion1);

  TestFuture<std::optional<base::FilePath>> file_path_future;
  cache_client()->GetCacheFilePath(web_bundle_id2, kVersion1,
                                   file_path_future.GetCallback());

  EXPECT_EQ(file_path_future.Get(), bundle_path2);
}

TEST_P(IwaCacheClientTest, IncorrectVersionParsed) {
  base::FilePath bundle_path1 =
      CreateBundleFile(web_bundle_id(), base::Version("aaaaa"));

  TestFuture<std::optional<base::FilePath>> file_path_future;
  cache_client()->GetCacheFilePath(web_bundle_id(), /*version=*/std::nullopt,
                                   file_path_future.GetCallback());

  EXPECT_EQ(file_path_future.Get(), std::nullopt);
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
