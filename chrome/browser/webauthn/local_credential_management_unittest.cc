// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "build/build_config.h"

// This class is only supported on Windows so far.
#if BUILDFLAG(IS_WIN)

#include "base/run_loop.h"
#include "chrome/browser/webauthn/local_credential_management.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kHasPlatformCredentialsPref[] =
    "webauthn.has_platform_credentials";
constexpr char kRpId[] = "example.com";
constexpr uint8_t kCredId[] = {1, 2, 3, 4};

class LocalCredentialManagementTest : public testing::Test {
 protected:
  void SetUp() override {
    LocalCredentialManagement::RegisterProfilePrefs(
        profile_.GetTestingPrefService()->registry());
    api_.set_supports_silent_discovery(true);
  }

  bool HasCredentials() {
    device::test::TestCallbackReceiver<bool> callback;
    local_cred_man_.HasCredentials(&profile_, callback.callback());

    while (!callback.was_called()) {
      base::RunLoop().RunUntilIdle();
    }
    return std::get<0>(callback.TakeResult());
  }

  absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
  Enumerate() {
    device::test::TestCallbackReceiver<
        absl::optional<std::vector<device::DiscoverableCredentialMetadata>>>
        callback;
    local_cred_man_.Enumerate(&profile_, callback.callback());

    while (!callback.was_called()) {
      base::RunLoop().RunUntilIdle();
    }
    return std::get<0>(callback.TakeResult());
  }

  // A `BrowserTaskEnvironment` needs to be in-scope in order to create a
  // `TestingProfile`.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  device::FakeWinWebAuthnApi api_;
  LocalCredentialManagement local_cred_man_ = {&api_};
};

TEST_F(LocalCredentialManagementTest, NoSupport) {
  // With no support, `HasCredentials` should return false and `Enumerate`
  // should return no value.
  api_.set_supports_silent_discovery(false);

  EXPECT_FALSE(HasCredentials());
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kHasPlatformCredentialsPref));

  EXPECT_FALSE(Enumerate().has_value());
}

TEST_F(LocalCredentialManagementTest, NoCredentials) {
  // With support but no credentials `HasCredentials` should still return false,
  // but `Enumerate` should return an empty list.
  EXPECT_FALSE(HasCredentials());
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kHasPlatformCredentialsPref));

  const absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
      result = Enumerate();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(LocalCredentialManagementTest, OneCredential) {
  // With a credential injected, `HasCredentials` should return true and should
  // cache that in the profile. Enumerate should return that credential.
  api_.InjectDiscoverableCredential(
      kCredId, {kRpId, absl::nullopt, absl::nullopt},
      {{1, 2, 3, 4}, absl::nullopt, absl::nullopt, absl::nullopt});

  EXPECT_TRUE(HasCredentials());
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kHasPlatformCredentialsPref));

  const absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
      result = Enumerate();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0).rp_id, kRpId);
}

TEST_F(LocalCredentialManagementTest, CacheIsUsed) {
  // If the cache is set to true then HasCredentials will return true even
  // though there aren't any credentials.
  profile_.GetPrefs()->SetBoolean(kHasPlatformCredentialsPref, true);

  EXPECT_TRUE(HasCredentials());
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kHasPlatformCredentialsPref));

  const absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
      result = Enumerate();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());

  // Calling `Enumerate` should have updated the cache to reflect the fact that
  // there aren't any credentials.
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kHasPlatformCredentialsPref));
}

}  // namespace

#endif
