// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_auditor_impl.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using AssistantActivityControlConsent =
    sync_pb::UserConsentTypes::AssistantActivityControlConsent;
using SyncConsent = sync_pb::UserConsentTypes::SyncConsent;
using sync_pb::UserConsentSpecifics;
using sync_pb::UserConsentTypes;
using ::testing::ElementsAreArray;

namespace consent_auditor {

namespace {

// Fake product locate for testing.
constexpr char kCurrentAppLocale[] = "en-US";

// Fake message ids.
constexpr std::array<int, 3> kDescriptionMessageIds = {12, 37, 42};
constexpr int kConfirmationMessageId = 47;

class FakeConsentSyncBridge : public ConsentSyncBridge {
 public:
  ~FakeConsentSyncBridge() override = default;

  // ConsentSyncBridge implementation.
  void RecordConsent(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics) override {
    DCHECK(specifics);
    recorded_user_consents_.push_back(*specifics);
  }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    return delegate_;
  }

  // Fake methods.
  void SetControllerDelegate(
      base::WeakPtr<syncer::DataTypeControllerDelegate> delegate) {
    delegate_ = delegate;
  }

  const std::vector<UserConsentSpecifics>& GetRecordedUserConsents() const {
    return recorded_user_consents_;
  }

 private:
  base::WeakPtr<syncer::DataTypeControllerDelegate> delegate_;
  std::vector<UserConsentSpecifics> recorded_user_consents_;
};

}  // namespace

class ConsentAuditorImplTest : public testing::Test {
 public:
  // Fake account ID for testing.
  const CoreAccountId kAccountId;

  ConsentAuditorImplTest()
      : kAccountId(CoreAccountId::FromGaiaId("testing_account_id")) {}

  void SetUp() override {
    CreateConsentAuditorImpl(std::make_unique<FakeConsentSyncBridge>());
  }

  void CreateConsentAuditorImpl(std::unique_ptr<FakeConsentSyncBridge> bridge) {
    consent_sync_bridge_ = bridge.get();
    consent_auditor_ = std::make_unique<ConsentAuditorImpl>(
        std::move(bridge), kCurrentAppLocale, clock());
  }

  base::SimpleTestClock* clock() { return &test_clock_; }
  ConsentAuditorImpl* consent_auditor() { return consent_auditor_.get(); }
  FakeConsentSyncBridge* consent_sync_bridge() {
    return consent_sync_bridge_.get();
  }

 private:
  // Test helpers.
  base::SimpleTestClock test_clock_;
  raw_ptr<FakeConsentSyncBridge, DanglingUntriaged> consent_sync_bridge_ =
      nullptr;

  // Test object to be tested.
  std::unique_ptr<ConsentAuditorImpl> consent_auditor_;
};

TEST_F(ConsentAuditorImplTest, RecordGaiaConsentAsUserConsent) {
  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  clock()->SetNow(now);

  SyncConsent sync_consent;
  sync_consent.set_status(UserConsentTypes::GIVEN);
  sync_consent.set_confirmation_grd_id(kConfirmationMessageId);
  for (int id : kDescriptionMessageIds) {
    sync_consent.add_description_grd_ids(id);
  }
  consent_auditor()->RecordSyncConsent(kAccountId, sync_consent);

  std::vector<UserConsentSpecifics> consents =
      consent_sync_bridge()->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  const UserConsentSpecifics& consent = consents[0];

  EXPECT_EQ(now.since_origin().InMicroseconds(),
            consent.client_consent_time_usec());
  EXPECT_EQ(kAccountId.ToString(), consent.account_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());

  EXPECT_TRUE(consent.has_sync_consent());
  const SyncConsent& actual_sync_consent = consent.sync_consent();
  EXPECT_THAT(actual_sync_consent.description_grd_ids(),
              ElementsAreArray(kDescriptionMessageIds));
  EXPECT_EQ(actual_sync_consent.confirmation_grd_id(), kConfirmationMessageId);
}

TEST_F(ConsentAuditorImplTest, RecordArcPlayConsentRevocation) {
  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  clock()->SetNow(now);

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(UserConsentTypes::NOT_GIVEN);
  play_consent.set_confirmation_grd_id(kConfirmationMessageId);
  for (int id : kDescriptionMessageIds) {
    play_consent.add_description_grd_ids(id);
  }
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETTING_CHANGE);
  consent_auditor()->RecordArcPlayConsent(kAccountId, play_consent);

  const std::vector<UserConsentSpecifics> consents =
      consent_sync_bridge()->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  const UserConsentSpecifics& consent = consents[0];

  EXPECT_EQ(kAccountId.ToString(), consent.account_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());

  EXPECT_TRUE(consent.has_arc_play_terms_of_service_consent());
  const ArcPlayTermsOfServiceConsent& actual_play_consent =
      consent.arc_play_terms_of_service_consent();
  EXPECT_EQ(UserConsentTypes::NOT_GIVEN, actual_play_consent.status());
  EXPECT_EQ(ArcPlayTermsOfServiceConsent::SETTING_CHANGE,
            actual_play_consent.consent_flow());
  EXPECT_THAT(actual_play_consent.description_grd_ids(),
              ElementsAreArray(kDescriptionMessageIds));
  EXPECT_EQ(kConfirmationMessageId, actual_play_consent.confirmation_grd_id());
}

TEST_F(ConsentAuditorImplTest, RecordArcPlayConsent) {
  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  clock()->SetNow(now);

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(UserConsentTypes::GIVEN);
  play_consent.set_confirmation_grd_id(kConfirmationMessageId);
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETUP);

  // Verify the hash: 2fd4e1c6 7a2d28fc ed849ee1 bb76e739 1b93eb12.
  const uint8_t play_tos_hash[] = {0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28,
                                   0xfc, 0xed, 0x84, 0x9e, 0xe1, 0xbb, 0x76,
                                   0xe7, 0x39, 0x1b, 0x93, 0xeb, 0x12};
  play_consent.set_play_terms_of_service_hash(std::string(
      reinterpret_cast<const char*>(play_tos_hash), base::kSHA1Length));
  play_consent.set_play_terms_of_service_text_length(7);

  consent_auditor()->RecordArcPlayConsent(kAccountId, play_consent);

  const std::vector<UserConsentSpecifics> consents =
      consent_sync_bridge()->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  const UserConsentSpecifics& consent = consents[0];

  EXPECT_EQ(kAccountId.ToString(), consent.account_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());

  EXPECT_TRUE(consent.has_arc_play_terms_of_service_consent());
  const ArcPlayTermsOfServiceConsent& actual_play_consent =
      consent.arc_play_terms_of_service_consent();

  EXPECT_EQ(7, actual_play_consent.play_terms_of_service_text_length());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(play_tos_hash),
                        base::kSHA1Length),
            actual_play_consent.play_terms_of_service_hash());

  EXPECT_EQ(kConfirmationMessageId, actual_play_consent.confirmation_grd_id());
  EXPECT_EQ(ArcPlayTermsOfServiceConsent::SETUP,
            actual_play_consent.consent_flow());
  EXPECT_EQ(UserConsentTypes::GIVEN, actual_play_consent.status());
}

TEST_F(ConsentAuditorImplTest, ShouldReturnSyncDelegateWhenBridgePresent) {
  auto fake_bridge = std::make_unique<FakeConsentSyncBridge>();

  syncer::FakeDataTypeControllerDelegate fake_delegate(
      syncer::DataType::USER_CONSENTS);
  auto expected_delegate_ptr = fake_delegate.GetWeakPtr();
  DCHECK(expected_delegate_ptr);
  fake_bridge->SetControllerDelegate(expected_delegate_ptr);
  CreateConsentAuditorImpl(std::move(fake_bridge));

  // There is a bridge (i.e. separate sync type for consents is enabled), thus,
  // there should be a delegate as well.
  EXPECT_EQ(expected_delegate_ptr.get(),
            consent_auditor()->GetControllerDelegate().get());
}

TEST_F(ConsentAuditorImplTest, RecordAssistantActivityControlConsent) {
  constexpr char ui_audit_key[] = {0x67, 0x23, 0x78};

  AssistantActivityControlConsent assistant_consent;
  assistant_consent.set_status(UserConsentTypes::GIVEN);
  assistant_consent.set_ui_audit_key(std::string(ui_audit_key, 3));
  assistant_consent.set_setting_type(AssistantActivityControlConsent::ALL);

  consent_auditor()->RecordAssistantActivityControlConsent(kAccountId,
                                                           assistant_consent);

  std::vector<UserConsentSpecifics> consents =
      consent_sync_bridge()->GetRecordedUserConsents();
  ASSERT_EQ(consents.size(), 1u);
  const UserConsentSpecifics& consent = consents[0];

  EXPECT_EQ(kAccountId.ToString(), consent.account_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());

  EXPECT_TRUE(consent.has_assistant_activity_control_consent());
  EXPECT_EQ(UserConsentTypes::GIVEN,
            consent.assistant_activity_control_consent().status());
  EXPECT_EQ(std::string(ui_audit_key, 3),
            consent.assistant_activity_control_consent().ui_audit_key());
  EXPECT_EQ(AssistantActivityControlConsent::ALL,
            consent.assistant_activity_control_consent().setting_type());
}

}  // namespace consent_auditor
