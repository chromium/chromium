// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/mechanism_sorter.h"

#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/fido_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
using CredentialInfo = Mechanism::CredentialInfo;
using PasswordInfo = Mechanism::PasswordInfo;

const auto kUserId = std::vector<uint8_t>{0x01, 0x02, 0x03};

// Helper to create a GPM Passkey mechanism.
Mechanism CreateEnclavePasskey(const std::u16string& user_name,
                               std::optional<base::Time> last_used_time) {
  Mechanism::Credential cred_info(
      {device::AuthenticatorType::kEnclave, kUserId, last_used_time});
  return Mechanism(std::move(cred_info), user_name, user_name, kSmartphoneIcon,
                   base::DoNothing());
}

// Helper to create a Platform Passkey mechanism.
Mechanism CreatePlatformPasskey(const std::u16string& user_name,
                                std::optional<base::Time> last_used_time) {
  Mechanism::Credential cred_info(
      {device::AuthenticatorType::kICloudKeychain, kUserId, last_used_time});
  return Mechanism(std::move(cred_info), user_name, user_name, kSmartphoneIcon,
                   base::DoNothing());
}

// Helper to create a Password mechanism.
Mechanism CreatePassword(const std::u16string& user_name,
                         base::Time last_used_time) {
  Mechanism::Type password_data =
      Mechanism::Password(Mechanism::PasswordInfo(last_used_time));
  return Mechanism(std::move(password_data), user_name, user_name,
                   kSmartphoneIcon, base::DoNothing());
}

class MechanismSorterTest : public ::testing::Test {
 public:
  MechanismSorterTest() = default;

 protected:
  MechanismSorter sorter_;
};

// Test that an empty list remains empty.
TEST_F(MechanismSorterTest, EmptyList) {
  std::vector<Mechanism> mechanisms;
  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  EXPECT_TRUE(result.empty());
}

// Test that a list with one enclave passkey remains unchanged.
TEST_F(MechanismSorterTest, SingleEnclaveMechanism) {
  std::vector<Mechanism> mechanisms;
  mechanisms.push_back(CreateEnclavePasskey(u"user1", base::Time::Now()));
  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].name, u"user1");
}

// Test that a list with one platform passkey remains unchanged.
TEST_F(MechanismSorterTest, SinglePlatformMechanism) {
  std::vector<Mechanism> mechanisms;
  mechanisms.push_back(CreatePlatformPasskey(u"user1", std::nullopt));
  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].name, u"user1");
}

// Test that a list with one password remains unchanged.
TEST_F(MechanismSorterTest, SinglePasswordMechanism) {
  std::vector<Mechanism> mechanisms;
  mechanisms.push_back(CreatePassword(u"user1", base::Time::Now()));
  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].name, u"user1");
}

// Test deduplication: GPM Passkey preferred over Platform Passkey if newer.
TEST_F(MechanismSorterTest, DeduplicateGpmPasskeyVsPlatformPasskey_GpmNewer) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  mechanisms.push_back(CreatePlatformPasskey(u"user1", time_older));
  mechanisms.push_back(CreateEnclavePasskey(u"user1", time_now));  // Newer

  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(std::get<Mechanism::Credential>(result[0].type).value().source,
            device::AuthenticatorType::kEnclave);
}

// Test deduplication: GPM Passkey preferred over Password if GPM Passkey is
// newer.
TEST_F(MechanismSorterTest, DeduplicateGpmPasskeyVsGpmPassword_PasskeyNewer) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  mechanisms.push_back(CreatePassword(u"user1", time_older));
  mechanisms.push_back(CreateEnclavePasskey(u"user1", time_now));  // Newer

  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Mechanism::Credential>(result[0].type));
  EXPECT_EQ(std::get<Mechanism::Credential>(result[0].type).value().source,
            device::AuthenticatorType::kEnclave);
}

// Test deduplication: GPM Password preferred over GPM Passkey if Password
// is newer.
TEST_F(MechanismSorterTest, DeduplicateGpmPasskeyVsGpmPassword_PasswordNewer) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  mechanisms.push_back(CreateEnclavePasskey(u"user1", time_older));
  mechanisms.push_back(CreatePassword(u"user1", time_now));

  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Mechanism::Password>(result[0].type));
}

// Test deduplication: Platform Passkey preferred over Password.
TEST_F(MechanismSorterTest, DeduplicatePlatformPasskeyVsGpmPassword) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();

  mechanisms.push_back(CreatePassword(u"user1", time_now));
  mechanisms.push_back(CreatePlatformPasskey(u"user1", std::nullopt));

  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Mechanism::Credential>(result[0].type));
  EXPECT_NE(std::get<Mechanism::Credential>(result[0].type).value().source,
            device::AuthenticatorType::kEnclave);
}

// Test sorting: Most recent first.
TEST_F(MechanismSorterTest, SortByTimestamp) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);
  base::Time time_oldest = time_older - base::Minutes(1);

  mechanisms.push_back(CreateEnclavePasskey(u"user_c", time_older));
  mechanisms.push_back(CreateEnclavePasskey(u"user_a", time_now));
  mechanisms.push_back(CreateEnclavePasskey(u"user_b", time_oldest));

  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].name, u"user_a");
  EXPECT_EQ(result[1].name, u"user_c");
  EXPECT_EQ(result[2].name, u"user_b");
}

// Test sorting: Alphabetical by name if timestamps are equal.
TEST_F(MechanismSorterTest, SortByNameIfTimestampsEqual) {
  std::vector<Mechanism> mechanisms;
  base::Time same_time = base::Time::Now();

  mechanisms.push_back(CreateEnclavePasskey(u"user_c", same_time));
  mechanisms.push_back(CreateEnclavePasskey(u"user_a", same_time));
  mechanisms.push_back(CreateEnclavePasskey(u"user_b", same_time));

  std::vector<Mechanism> result = sorter_.ProcessMechanisms(
      std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].name, u"user_a");
  EXPECT_EQ(result[1].name, u"user_b");
  EXPECT_EQ(result[2].name, u"user_c");
}

// Test that sorting/deduplication does not happen for non-kModalImmediate UI.
TEST_F(MechanismSorterTest, NoProcessingForOtherUIPresentations) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  // Order is intentionally "wrong" for kModalImmediate
  mechanisms.push_back(CreateEnclavePasskey(u"user1", time_older));
  mechanisms.push_back(CreatePlatformPasskey(u"user1", std::nullopt));
  mechanisms.push_back(CreateEnclavePasskey(u"user2", time_now));

  std::vector<Mechanism> result =
      sorter_.ProcessMechanisms(std::move(mechanisms), UIPresentation::kModal);

  ASSERT_EQ(result.size(), 3u);
  // Expect original order and content
  EXPECT_EQ(result[0].name, u"user1");
  EXPECT_EQ(result[1].name, u"user1");
  EXPECT_EQ(result[2].name, u"user2");
}

}  // namespace
