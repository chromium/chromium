// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/credential_sorter_desktop.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/fido_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
using CredentialInfo = Mechanism::CredentialInfo;
using PasswordInfo = Mechanism::PasswordInfo;

namespace webauthn::sorting {

namespace {

const auto kUserId = std::vector<uint8_t>{0x01, 0x02, 0x03};

// Helper to create a GPM Passkey mechanism.
Mechanism CreateGpmPasskey(const std::u16string& user_name,
                           std::optional<base::Time> last_used_time) {
  Mechanism::Credential cred_info(
      {device::AuthenticatorType::kEnclave, kUserId, last_used_time});
  return Mechanism(std::move(cred_info), user_name, kSmartphoneIcon,
                   base::DoNothing());
}

// Helper to create a Platform Passkey mechanism.
Mechanism CreatePlatformPasskey(const std::u16string& user_name,
                                std::optional<base::Time> last_used_time) {
  Mechanism::Credential cred_info(
      {device::AuthenticatorType::kICloudKeychain, kUserId, last_used_time});
  return Mechanism(std::move(cred_info), user_name, kSmartphoneIcon,
                   base::DoNothing());
}

// Helper to create a Password mechanism.
Mechanism CreatePassword(const std::u16string& user_name,
                         base::Time last_used_time) {
  Mechanism::Type password_data =
      Mechanism::Password(Mechanism::PasswordInfo(last_used_time));
  return Mechanism(std::move(password_data), user_name, kSmartphoneIcon,
                   base::DoNothing());
}

}  // namespace

class CredentialSorterDesktopTest : public ::testing::Test {
 public:
  CredentialSorterDesktopTest() = default;

 protected:
  void ExpectNoDeduplication() {
    histogram_tester_.ExpectUniqueSample(
        "WebAuthentication.MechanismSorter.DeduplicationHappened", false, 1);
    histogram_tester_.ExpectTotalCount(
        "WebAuthentication.MechanismSorter.SelectedMechanismType", 0);
  }

  void ExpectDeduplicationRecorded(WebAuthnDeduplicatedType deduplicated_type) {
    histogram_tester_.ExpectUniqueSample(
        "WebAuthentication.MechanismSorter.DeduplicationHappened", true, 1);
    histogram_tester_.ExpectUniqueSample(
        "WebAuthentication.MechanismSorter.SelectedMechanismType",
        deduplicated_type, 1);
  }

  base::HistogramTester histogram_tester_;
};

// Test that an empty list remains empty.
TEST_F(CredentialSorterDesktopTest, EmptyList) {
  std::vector<Mechanism> mechanisms;
  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  EXPECT_TRUE(result.empty());
  ExpectNoDeduplication();
}

// Test that a list with one GPM passkey remains unchanged.
TEST_F(CredentialSorterDesktopTest, SingleGpmMechanism) {
  std::vector<Mechanism> mechanisms;
  mechanisms.push_back(CreateGpmPasskey(u"user1", base::Time::Now()));
  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].name, u"user1");
  ExpectNoDeduplication();
}

// Test that a list with one platform passkey remains unchanged.
TEST_F(CredentialSorterDesktopTest, SinglePlatformMechanism) {
  std::vector<Mechanism> mechanisms;
  mechanisms.push_back(CreatePlatformPasskey(u"user1", std::nullopt));
  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].name, u"user1");
  ExpectNoDeduplication();
}

// Test that a list with one password remains unchanged.
TEST_F(CredentialSorterDesktopTest, SinglePasswordMechanism) {
  std::vector<Mechanism> mechanisms;
  mechanisms.push_back(CreatePassword(u"user1", base::Time::Now()));
  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0].name, u"user1");
  ExpectNoDeduplication();
}

// Test deduplication: GPM Passkey preferred over Platform Passkey if newer.
TEST_F(CredentialSorterDesktopTest,
       DeduplicateGpmPasskeyVsPlatformPasskey_GpmNewer) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  mechanisms.push_back(CreatePlatformPasskey(u"user1", time_older));
  mechanisms.push_back(CreateGpmPasskey(u"user1", time_now));  // Newer

  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(std::get<Mechanism::Credential>(result[0].type).value().source,
            device::AuthenticatorType::kEnclave);
  ExpectDeduplicationRecorded(WebAuthnDeduplicatedType::kGpmPasskey);
}

// Test deduplication: GPM Passkey preferred over Password if GPM Passkey is
// newer.
TEST_F(CredentialSorterDesktopTest,
       DeduplicateGpmPasskeyVsGpmPassword_PasskeyNewer) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  mechanisms.push_back(CreatePassword(u"user1", time_older));
  mechanisms.push_back(CreateGpmPasskey(u"user1", time_now));  // Newer

  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Mechanism::Credential>(result[0].type));
  EXPECT_EQ(std::get<Mechanism::Credential>(result[0].type).value().source,
            device::AuthenticatorType::kEnclave);
  ExpectDeduplicationRecorded(WebAuthnDeduplicatedType::kGpmPasskey);
}

// Test deduplication: GPM Password preferred over GPM Passkey if Password
// is newer.
TEST_F(CredentialSorterDesktopTest,
       DeduplicateGpmPasskeyVsGpmPassword_PasswordNewer) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  mechanisms.push_back(CreateGpmPasskey(u"user1", time_older));
  mechanisms.push_back(CreatePassword(u"user1", time_now));

  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Mechanism::Password>(result[0].type));
  ExpectDeduplicationRecorded(WebAuthnDeduplicatedType::kPassword);
}

// Test deduplication: Platform Passkey preferred over Password.
TEST_F(CredentialSorterDesktopTest, DeduplicatePlatformPasskeyVsGpmPassword) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();

  mechanisms.push_back(CreatePassword(u"user1", time_now));
  mechanisms.push_back(CreatePlatformPasskey(u"user1", std::nullopt));

  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Mechanism::Credential>(result[0].type));
  EXPECT_NE(std::get<Mechanism::Credential>(result[0].type).value().source,
            device::AuthenticatorType::kEnclave);
  ExpectDeduplicationRecorded(WebAuthnDeduplicatedType::kPlatformPasskey);
}

// Test sorting: Most recent first.
TEST_F(CredentialSorterDesktopTest, SortByTimestamp) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);
  base::Time time_oldest = time_older - base::Minutes(1);

  mechanisms.push_back(CreateGpmPasskey(u"user_c", time_older));
  mechanisms.push_back(CreateGpmPasskey(u"user_a", time_now));
  mechanisms.push_back(CreateGpmPasskey(u"user_b", time_oldest));

  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].name, u"user_a");
  EXPECT_EQ(result[1].name, u"user_c");
  EXPECT_EQ(result[2].name, u"user_b");
  ExpectNoDeduplication();
}

// Test sorting: Alphabetical by name if timestamps are equal.
TEST_F(CredentialSorterDesktopTest, SortByNameIfTimestampsEqual) {
  std::vector<Mechanism> mechanisms;
  base::Time same_time = base::Time::Now();

  mechanisms.push_back(CreateGpmPasskey(u"user_c", same_time));
  mechanisms.push_back(CreateGpmPasskey(u"user_a", same_time));
  mechanisms.push_back(CreateGpmPasskey(u"user_b", same_time));

  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].name, u"user_a");
  EXPECT_EQ(result[1].name, u"user_b");
  EXPECT_EQ(result[2].name, u"user_c");
  ExpectNoDeduplication();
}

// Test that sorting/deduplication does not happen for non-kModalImmediate UI.
TEST_F(CredentialSorterDesktopTest, NoProcessingForOtherUIPresentations) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  // Order is intentionally "wrong" for kModalImmediate
  mechanisms.push_back(CreateGpmPasskey(u"user1", time_older));
  mechanisms.push_back(CreatePlatformPasskey(u"user1", std::nullopt));
  mechanisms.push_back(CreateGpmPasskey(u"user2", time_now));

  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModal);

  ASSERT_EQ(result.size(), 3u);
  // Expect original order and content
  EXPECT_EQ(result[0].name, u"user1");
  EXPECT_EQ(result[1].name, u"user1");
  EXPECT_EQ(result[2].name, u"user2");
  histogram_tester_.ExpectTotalCount(
      "WebAuthentication.MechanismSorter.DeduplicationHappened", 0);
  histogram_tester_.ExpectTotalCount(
      "WebAuthentication.MechanismSorter.SelectedMechanismType", 0);
}

// Test multiple deduplications in one call.
TEST_F(CredentialSorterDesktopTest, MultipleDeduplications) {
  std::vector<Mechanism> mechanisms;
  base::Time time_now = base::Time::Now();
  base::Time time_older = time_now - base::Minutes(1);

  // User 1: platform passkey wins over password.
  mechanisms.push_back(CreatePassword(u"user1", time_now));
  mechanisms.push_back(CreatePlatformPasskey(u"user1", time_older));

  // User 2: newer GPM passkey wins over password.
  mechanisms.push_back(CreatePassword(u"user2", time_older));
  mechanisms.push_back(CreateGpmPasskey(u"user2", time_now));

  std::vector<Mechanism> result =
      SortMechanisms(std::move(mechanisms), UIPresentation::kModalImmediate);
  ASSERT_EQ(result.size(), 2u);

  histogram_tester_.ExpectUniqueSample(
      "WebAuthentication.MechanismSorter.DeduplicationHappened", true, 1);
  histogram_tester_.ExpectTotalCount(
      "WebAuthentication.MechanismSorter.SelectedMechanismType", 2);
  histogram_tester_.ExpectBucketCount(
      "WebAuthentication.MechanismSorter.SelectedMechanismType",
      WebAuthnDeduplicatedType::kPlatformPasskey, 1);
  histogram_tester_.ExpectBucketCount(
      "WebAuthentication.MechanismSorter.SelectedMechanismType",
      WebAuthnDeduplicatedType::kGpmPasskey, 1);
}

}  // namespace webauthn::sorting
