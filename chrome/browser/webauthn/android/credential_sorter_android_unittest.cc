// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/android/credential_sorter_android.h"

#include "base/time/time.h"
#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using Credential = TouchToFillView::Credential;

using password_manager::PasskeyCredential;
using password_manager::UiCredential;
using webauthn::sorting::SortTouchToFillCredentials;

namespace webauthn::sorting {

namespace {

PasskeyCredential CreatePasskey(const std::string& username,
                                std::optional<base::Time> last_used_time) {
  return PasskeyCredential(PasskeyCredential::Source::kAndroidPhone,
                           PasskeyCredential::RpId("rp_id"),
                           PasskeyCredential::CredentialId({1, 2, 3, 4}),
                           PasskeyCredential::UserId({5, 6, 7, 8}),
                           PasskeyCredential::Username(username),
                           PasskeyCredential::DisplayName(""),
                           /* creation_time= */ std::nullopt, last_used_time);
}

UiCredential CreatePassword(const std::u16string& user_name,
                            base::Time last_used_time) {
  return UiCredential(user_name, /*password=*/u"hunter2",
                      url::Origin::Create(GURL("")), "display_name",
                      password_manager_util::GetLoginMatchType::kGrouped,
                      last_used_time, UiCredential::IsBackupCredential(false));
}

}  // namespace

// This tests the SortTouchToFillCredentials() function.
class CredentialSorterAndroidTest : public ::testing::Test {
 public:
  CredentialSorterAndroidTest() = default;
};

// Test that an empty list remains empty.
TEST_F(CredentialSorterAndroidTest, EmptyList) {
  std::vector<Credential> credentials;
  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ true);
  EXPECT_TRUE(result.empty());
}

// Test that a list with one passkey remains unchanged.
TEST_F(CredentialSorterAndroidTest, SinglePasskey) {
  std::vector<Credential> credentials;
  credentials.push_back(CreatePasskey("user1", std::nullopt));
  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ true);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(std::get<PasskeyCredential>(result[0]).username(), "user1");
}

// Test that a list with one password remains unchanged.
TEST_F(CredentialSorterAndroidTest, SinglePassword) {
  std::vector<Credential> credentials;
  credentials.push_back(CreatePassword(u"user1", base::Time::Now()));
  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ true);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(std::get<UiCredential>(result[0]).username(), u"user1");
}

// Deduplication: Passkey preferred over Password if Passkey is newer.
TEST_F(CredentialSorterAndroidTest, DeduplicatePasskeyVsPassword_PasskeyNewer) {
  std::vector<Credential> credentials;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  credentials.push_back(CreatePassword(u"user1", time_older));
  credentials.push_back(CreatePasskey("user1", time_now));  // Newer

  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ true);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<PasskeyCredential>(result[0]));
}

// Deduplication: Password preferred over Passkey if Password is newer.
TEST_F(CredentialSorterAndroidTest,
       DeduplicatePasskeyVsPassword_PasswordNewer) {
  std::vector<Credential> credentials;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  credentials.push_back(CreatePasskey("user1", time_older));
  credentials.push_back(CreatePassword(u"user1", time_now));

  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ true);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<UiCredential>(result[0]));
}

// Sorting: Most recent first.
TEST_F(CredentialSorterAndroidTest, SortByTimestamp) {
  std::vector<Credential> credentials;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);
  base::Time time_oldest = time_older - base::Minutes(1);

  credentials.push_back(CreatePasskey("user_c", time_older));
  credentials.push_back(CreatePasskey("user_a", time_now));
  credentials.push_back(CreatePasskey("user_b", time_oldest));

  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ true);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(std::get<PasskeyCredential>(result[0]).username(), "user_a");
  EXPECT_EQ(std::get<PasskeyCredential>(result[1]).username(), "user_c");
  EXPECT_EQ(std::get<PasskeyCredential>(result[2]).username(), "user_b");
}

// Test sorting: Alphabetical by name if timestamps are equal.
TEST_F(CredentialSorterAndroidTest, SortByNameIfTimestampsEqual) {
  std::vector<Credential> credentials;
  base::Time same_time = base::Time::Now();

  credentials.push_back(CreatePasskey("user_c", same_time));
  credentials.push_back(CreatePasskey("user_a", same_time));
  credentials.push_back(CreatePasskey("user_b", same_time));

  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ true);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(std::get<PasskeyCredential>(result[0]).username(), "user_a");
  EXPECT_EQ(std::get<PasskeyCredential>(result[1]).username(), "user_b");
  EXPECT_EQ(std::get<PasskeyCredential>(result[2]).username(), "user_c");
}

// Test that sorting/deduplication does not happen for non-immediate UI.
TEST_F(CredentialSorterAndroidTest, NoProcessingForOtherUIPresentations) {
  std::vector<Credential> credentials;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  // Ordering that would be sorted in immediate.
  credentials.push_back(CreatePasskey("user1", time_older));
  credentials.push_back(CreatePasskey("user2", time_now));

  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ false);

  ASSERT_EQ(result.size(), 2u);
  // Expect original order and content
  EXPECT_EQ(std::get<PasskeyCredential>(result[0]).username(), "user1");
  EXPECT_EQ(std::get<PasskeyCredential>(result[1]).username(), "user2");
}

// Test multiple deduplications in one call.
TEST_F(CredentialSorterAndroidTest, MultipleDeduplications) {
  std::vector<TouchToFillView::Credential> credentials;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  // User 1: New password wins over passkey.
  credentials.push_back(CreatePassword(u"user1", time_now));
  credentials.push_back(CreatePasskey("user1", time_older));

  // User 2: Newer passkey wins over password.
  credentials.push_back(CreatePassword(u"user2", time_older));
  credentials.push_back(CreatePasskey("user2", time_now));

  std::vector<Credential> result = SortTouchToFillCredentials(
      std::move(credentials), /* immediate_ui_mode= */ true);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<UiCredential>(result[0]));
  EXPECT_TRUE(std::holds_alternative<PasskeyCredential>(result[1]));
  EXPECT_EQ(std::get<UiCredential>(result[0]).username(), u"user1");
  EXPECT_EQ(std::get<PasskeyCredential>(result[1]).username(), "user2");
}

}  // namespace webauthn::sorting
