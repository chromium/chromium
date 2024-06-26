// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/local_credential_management_win.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kHasPlatformCredentialsPref[] =
    "webauthn.has_platform_credentials";
constexpr char kRpId[] = "example.com";
constexpr uint8_t kCredId[] = {1, 2, 3, 4};

class LocalCredentialManagementTest : public testing::Test {
 protected:
  LocalCredentialManagementTest() = default;

  void SetUp() override { api_.set_supports_silent_discovery(true); }

  bool HasCredentials() {
    base::test::TestFuture<bool> future;
    local_cred_man_.HasCredentials(future.GetCallback());

    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  std::optional<std::vector<device::DiscoverableCredentialMetadata>>
  Enumerate() {
    base::test::TestFuture<
        std::optional<std::vector<device::DiscoverableCredentialMetadata>>>
        future;
    local_cred_man_.Enumerate(future.GetCallback());

    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  // A `BrowserTaskEnvironment` needs to be in-scope in order to create a
  // `TestingProfile`.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  device::FakeWinWebAuthnApi api_;
  LocalCredentialManagementWin local_cred_man_{&api_, &profile_};
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

  const std::optional<std::vector<device::DiscoverableCredentialMetadata>>
      result = Enumerate();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(LocalCredentialManagementTest, OneCredential) {
  // With a credential injected, `HasCredentials` should return true and should
  // cache that in the profile. Enumerate should return that credential.
  api_.InjectDiscoverableCredential(kCredId, {kRpId, std::nullopt},
                                    {{1, 2, 3, 4}, std::nullopt, std::nullopt});

  EXPECT_TRUE(HasCredentials());
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(kHasPlatformCredentialsPref));

  const std::optional<std::vector<device::DiscoverableCredentialMetadata>>
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

  const std::optional<std::vector<device::DiscoverableCredentialMetadata>>
      result = Enumerate();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());

  // Calling `Enumerate` should have updated the cache to reflect the fact that
  // there aren't any credentials.
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(kHasPlatformCredentialsPref));
}

TEST_F(LocalCredentialManagementTest, Sorting) {
  constexpr uint8_t kCredId1[] = {1};
  constexpr uint8_t kCredId2[] = {2};
  constexpr uint8_t kCredId3[] = {3};
  constexpr uint8_t kCredId4[] = {4};
  constexpr uint8_t kCredId5[] = {5};
  constexpr uint8_t kCredId6[] = {6};
  constexpr uint8_t kCredId7[] = {7};

  api_.InjectDiscoverableCredential(kCredId7, {"zzz.de", std::nullopt},
                                    {{1, 2, 3, 4}, "username", std::nullopt});
  api_.InjectDiscoverableCredential(kCredId2, {"zzz.de", std::nullopt},
                                    {{1, 2, 3, 4}, "username", std::nullopt});
  api_.InjectDiscoverableCredential(kCredId3,
                                    {"www.example.co.uk", std::nullopt},
                                    {{1, 2, 3, 4}, "user1", std::nullopt});
  api_.InjectDiscoverableCredential(kCredId4,
                                    {"foo.www.example.co.uk", std::nullopt},
                                    {{1, 2, 3, 4}, "user1", std::nullopt});
  api_.InjectDiscoverableCredential(kCredId5,
                                    {"foo.example.co.uk", std::nullopt},
                                    {{1, 2, 3, 4}, "user1", std::nullopt});
  api_.InjectDiscoverableCredential(kCredId6, {"aardvark.us", std::nullopt},
                                    {{1, 2, 3, 4}, "username", std::nullopt});
  api_.InjectDiscoverableCredential(kCredId1, {"example.co.uk", std::nullopt},
                                    {{1, 2, 3, 4}, "user2", std::nullopt});

  const std::vector<device::DiscoverableCredentialMetadata> result =
      Enumerate().value();
  ASSERT_EQ(result.size(), 7u);
  EXPECT_EQ(result[0].rp_id, "aardvark.us");
  // Despite starting with other characters, all entries for example.co.uk
  // should be sorted together.
  EXPECT_EQ(result[1].rp_id, "example.co.uk");
  EXPECT_EQ(result[2].rp_id, "foo.example.co.uk");
  EXPECT_EQ(result[3].rp_id, "www.example.co.uk");
  EXPECT_EQ(result[4].rp_id, "foo.www.example.co.uk");
  EXPECT_EQ(result[5].rp_id, "zzz.de");
  EXPECT_EQ(result[6].rp_id, "zzz.de");
  // The two zzz.de entries have the same RP ID and user.name, thus they should
  // be sorted by credential ID.
  EXPECT_EQ(result[6].cred_id[0], 7);
}

}  // namespace
