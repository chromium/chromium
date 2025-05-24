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

using SessionType = IwaCacheClient::SessionType;

// TODO(crbug.com/416006853): refactor tests to cover static functions.
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
      case SessionType::kKiosk:
        chromeos::SetUpFakeKioskSession();
        break;
      case SessionType::kManagedGuestSession:
        test_managed_guest_session_ = std::make_unique<
            profiles::testing::ScopedTestManagedGuestSession>();
        break;
    }

    ASSERT_TRUE(cache_root_dir_.CreateUniqueTempDir());
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, cache_root_dir_.GetPath());
  }

  const base::FilePath& CacheRootPath() { return cache_root_dir_.GetPath(); }

  SessionType GetSessionType() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebAppBundleCache};
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::ScopedUserManager user_manager_;
  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;

  // This is set only for MGS session.
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
};

}  // namespace web_app
