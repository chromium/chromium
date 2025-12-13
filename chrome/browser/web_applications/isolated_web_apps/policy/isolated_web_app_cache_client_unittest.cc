// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

enum class SessionType {
  kManagedGuestSession = 0,
  kIwaKiosk = 1,
  kWebKiosk = 2,
  kChromeAppKiosk = 3,
  kUserSession = 4,
};

// TODO(crbug.com/416006853): refactor tests to cover all static functions.
class IwaCacheClientTest : public ::testing::TestWithParam<SessionType> {
 public:
  IwaCacheClientTest() = default;
  IwaCacheClientTest(const IwaCacheClientTest&) = delete;
  IwaCacheClientTest& operator=(const IwaCacheClientTest&) = delete;
  ~IwaCacheClientTest() override = default;

  void SetUp() override {
    user_manager_.Reset(std::make_unique<user_manager::FakeUserManager>(
        TestingBrowserProcess::GetGlobal()->local_state()));

    switch (GetSessionType()) {
      case SessionType::kIwaKiosk:
        chromeos::SetUpFakeIwaKioskSession();
        break;
      case SessionType::kWebKiosk:
        chromeos::SetUpFakeWebKioskSession();
        break;
      case SessionType::kChromeAppKiosk:
        chromeos::SetUpFakeChromeAppKioskSession();
        break;
      case SessionType::kManagedGuestSession:
        test_managed_guest_session_ = std::make_unique<
            profiles::testing::ScopedTestManagedGuestSession>();
        break;
      case SessionType::kUserSession:
        // Do not setup regular user session.
        break;
    }
  }

  SessionType GetSessionType() { return GetParam(); }

 private:
  user_manager::ScopedUserManager user_manager_;

  // This is set only for MGS session.
  std::unique_ptr<profiles::testing::ScopedTestManagedGuestSession>
      test_managed_guest_session_;
};

TEST_P(IwaCacheClientTest, CachingIsEnabledByDefault) {
  EXPECT_TRUE(IsIwaBundleCacheFeatureEnabled());
}

TEST_P(IwaCacheClientTest, DisableExperiment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kIsolatedWebAppBundleCache);

  EXPECT_FALSE(IsIwaBundleCacheFeatureEnabled());
  EXPECT_FALSE(IsIwaBundleCacheEnabledInCurrentSession());
}

TEST_P(IwaCacheClientTest, CachingIsEnabledForSpecificSession) {
  EXPECT_TRUE(IsIwaBundleCacheFeatureEnabled());

  if (GetSessionType() == SessionType::kManagedGuestSession ||
      GetSessionType() == SessionType::kIwaKiosk) {
    EXPECT_TRUE(IsIwaBundleCacheEnabledInCurrentSession());
  } else {
    EXPECT_FALSE(IsIwaBundleCacheEnabledInCurrentSession());
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaCacheClientTest,
    testing::Values(SessionType::kManagedGuestSession,
                    SessionType::kIwaKiosk,
                    SessionType::kWebKiosk,
                    SessionType::kChromeAppKiosk,
                    SessionType::kUserSession));

}  // namespace web_app
