// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_auditor_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/consent_auditor/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/test/model/fake_model_type_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using AssistantActivityControlConsent =
    sync_pb::UserConsentTypes::AssistantActivityControlConsent;
using SyncConsent = sync_pb::UserConsentTypes::SyncConsent;
using sync_pb::UserConsentSpecifics;
using sync_pb::UserConsentTypes;

namespace consent_auditor {

namespace {

const char kLocalConsentDescriptionKey[] = "description";
const char kLocalConsentConfirmationKey[] = "confirmation";
const char kLocalConsentVersionKey[] = "version";
const char kLocalConsentLocaleKey[] = "locale";

// Fake product version for testing.
const char kCurrentAppVersion[] = "1.2.3.4";
const char kCurrentAppLocale[] = "en-US";

// A helper function to load the |description|, |confirmation|, |version|,
// and |locale|, in that order, from a record for the |feature| in
// the |consents| dictionary.
void LoadEntriesFromLocalConsentRecord(const base::Value* consents,
                                       const std::string& feature,
                                       std::string* description,
                                       std::string* confirmation,
                                       std::string* version,
                                       std::string* locale) {
  SCOPED_TRACE(::testing::Message() << "|feature| = " << feature);

  const base::Value* record =
      consents->FindKeyOfType(feature, base::Value::Type::DICTIONARY);
  ASSERT_TRUE(record);
  SCOPED_TRACE(::testing::Message() << "|record| = " << record);

  const base::Value* description_entry =
      record->FindKey(kLocalConsentDescriptionKey);
  const base::Value* confirmation_entry =
      record->FindKey(kLocalConsentConfirmationKey);
  const base::Value* version_entry = record->FindKey(kLocalConsentVersionKey);
  const base::Value* locale_entry = record->FindKey(kLocalConsentLocaleKey);

  ASSERT_TRUE(description_entry);
  ASSERT_TRUE(confirmation_entry);
  ASSERT_TRUE(version_entry);
  ASSERT_TRUE(locale_entry);

  *description = description_entry->GetString();
  *confirmation = confirmation_entry->GetString();
  *version = version_entry->GetString();
  *locale = locale_entry->GetString();
}

class FakeConsentSyncBridge : public ConsentSyncBridge {
 public:
  ~FakeConsentSyncBridge() override = default;

  // ConsentSyncBridge implementation.
  void RecordConsent(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics) override {
    DCHECK(specifics);
    recorded_user_consents_.push_back(*specifics);
  }

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override {
    return delegate_;
  }

  // Fake methods.
  void SetControllerDelegate(
      base::WeakPtr<syncer::ModelTypeControllerDelegate> delegate) {
    delegate_ = delegate;
  }

  const std::vector<UserConsentSpecifics>& GetRecordedUserConsents() const {
    return recorded_user_consents_;
  }

 private:
  base::WeakPtr<syncer::ModelTypeControllerDelegate> delegate_;
  std::vector<UserConsentSpecifics> recorded_user_consents_;
};

}  // namespace

class ConsentAuditorImplTest : public testing::Test {
 public:
  // Fake account ID for testing.
  const CoreAccountId kAccountId;

  ConsentAuditorImplTest() : kAccountId("testing_account_id") {}

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    // Use normal clock by default.
    clock_ = base::DefaultClock::GetInstance();
    consent_sync_bridge_ = std::make_unique<FakeConsentSyncBridge>();
    ConsentAuditorImpl::RegisterProfilePrefs(pref_service_->registry());
    app_version_ = kCurrentAppVersion;
    app_locale_ = kCurrentAppLocale;
    BuildConsentAuditorImpl();
  }

  // TODO(vitaliii): Add a real builder class instead.
  void BuildConsentAuditorImpl() {
    consent_auditor_ = std::make_unique<ConsentAuditorImpl>(
        pref_service_.get(), std::move(consent_sync_bridge_), app_version_,
        app_locale_, clock_);
  }

  // These have no effect before |BuildConsentAuditorImpl|.
  void SetAppVersion(const std::string& new_app_version) {
    app_version_ = new_app_version;
  }
  void SetAppLocale(const std::string& new_app_locale) {
    app_locale_ = new_app_locale;
  }
  void SetConsentSyncBridge(std::unique_ptr<ConsentSyncBridge> bridge) {
    consent_sync_bridge_ = std::move(bridge);
  }
  void SetClock(base::Clock* clock) { clock_ = clock; }

  ConsentAuditorImpl* consent_auditor() { return consent_auditor_.get(); }
  PrefService* pref_service() const { return pref_service_.get(); }

 private:
  std::unique_ptr<ConsentAuditorImpl> consent_auditor_;
  raw_ptr<base::Clock> clock_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::string app_version_;
  std::string app_locale_;
  std::unique_ptr<ConsentSyncBridge> consent_sync_bridge_;
};

TEST_F(ConsentAuditorImplTest, LocalConsentPrefRepresentation) {
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetConsentSyncBridge(std::make_unique<FakeConsentSyncBridge>());
  BuildConsentAuditorImpl();

  // No consents are written at first.
  EXPECT_FALSE(pref_service()->HasPrefPath(prefs::kLocalConsentsDictionary));

  // Record a consent and check that it appears in the prefs.
  const std::string kFeature1Description = "This will enable feature 1.";
  const std::string kFeature1Confirmation = "OK.";
  consent_auditor()->RecordLocalConsent("feature1", kFeature1Description,
                                        kFeature1Confirmation);
  ASSERT_TRUE(pref_service()->HasPrefPath(prefs::kLocalConsentsDictionary));
  const base::Value* consents =
      pref_service()->GetDictionary(prefs::kLocalConsentsDictionary);
  ASSERT_TRUE(consents);
  std::string description;
  std::string confirmation;
  std::string version;
  std::string locale;
  LoadEntriesFromLocalConsentRecord(consents, "feature1", &description,
                                    &confirmation, &version, &locale);
  EXPECT_EQ(kFeature1Description, description);
  EXPECT_EQ(kFeature1Confirmation, confirmation);
  EXPECT_EQ(kCurrentAppVersion, version);
  EXPECT_EQ(kCurrentAppLocale, locale);

  // Do the same for another feature.
  const std::string kFeature2Description = "Enable feature 2?";
  const std::string kFeature2Confirmation = "Yes.";
  consent_auditor()->RecordLocalConsent("feature2", kFeature2Description,
                                        kFeature2Confirmation);
  LoadEntriesFromLocalConsentRecord(consents, "feature2", &description,
                                    &confirmation, &version, &locale);
  EXPECT_EQ(kFeature2Description, description);
  EXPECT_EQ(kFeature2Confirmation, confirmation);
  EXPECT_EQ(kCurrentAppVersion, version);
  EXPECT_EQ(kCurrentAppLocale, locale);

  // They are two separate records; the latter did not overwrite the former.
  EXPECT_EQ(2u, consents->DictSize());
  EXPECT_TRUE(
      consents->FindKeyOfType("feature1", base::Value::Type::DICTIONARY));

  // Overwrite an existing consent, this time use a different product version
  // and a different locale.
  const std::string kFeature2NewDescription = "Re-enable feature 2?";
  const std::string kFeature2NewConfirmation = "Yes again.";
  const std::string kFeature2NewAppVersion = "5.6.7.8";
  const std::string kFeature2NewAppLocale = "de";
  SetAppVersion(kFeature2NewAppVersion);
  SetAppLocale(kFeature2NewAppLocale);
  SetConsentSyncBridge(std::make_unique<FakeConsentSyncBridge>());
  // We rebuild consent auditor to emulate restarting Chrome. This is the only
  // way to change app version or app locale.
  BuildConsentAuditorImpl();

  consent_auditor()->RecordLocalConsent("feature2", kFeature2NewDescription,
                                        kFeature2NewConfirmation);
  LoadEntriesFromLocalConsentRecord(consents, "feature2", &description,
                                    &confirmation, &version, &locale);
  EXPECT_EQ(kFeature2NewDescription, description);
  EXPECT_EQ(kFeature2NewConfirmation, confirmation);
  EXPECT_EQ(kFeature2NewAppVersion, version);
  EXPECT_EQ(kFeature2NewAppLocale, locale);

  // We still have two records.
  EXPECT_EQ(2u, consents->DictSize());
}

TEST_F(ConsentAuditorImplTest, RecordGaiaConsentAsUserConsent) {
  auto wrapped_fake_bridge = std::make_unique<FakeConsentSyncBridge>();
  FakeConsentSyncBridge* fake_bridge = wrapped_fake_bridge.get();
  base::SimpleTestClock test_clock;

  SetConsentSyncBridge(std::move(wrapped_fake_bridge));
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetClock(&test_clock);
  BuildConsentAuditorImpl();

  std::vector<int> kDescriptionMessageIds = {12, 37, 42};
  int kConfirmationMessageId = 47;

  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  test_clock.SetNow(now);

  SyncConsent sync_consent;
  sync_consent.set_status(UserConsentTypes::GIVEN);
  sync_consent.set_confirmation_grd_id(kConfirmationMessageId);
  for (int id : kDescriptionMessageIds) {
    sync_consent.add_description_grd_ids(id);
  }
  consent_auditor()->RecordSyncConsent(kAccountId, sync_consent);

  std::vector<UserConsentSpecifics> consents =
      fake_bridge->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  UserConsentSpecifics consent = consents[0];

  EXPECT_EQ(now.since_origin().InMicroseconds(),
            consent.client_consent_time_usec());
  EXPECT_EQ(kAccountId.ToString(), consent.account_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());

  EXPECT_TRUE(consent.has_sync_consent());
  const SyncConsent& actual_sync_consent = consent.sync_consent();
  EXPECT_EQ(3, actual_sync_consent.description_grd_ids_size());
  EXPECT_EQ(kDescriptionMessageIds[0],
            actual_sync_consent.description_grd_ids(0));
  EXPECT_EQ(kDescriptionMessageIds[1],
            actual_sync_consent.description_grd_ids(1));
  EXPECT_EQ(kDescriptionMessageIds[2],
            actual_sync_consent.description_grd_ids(2));
  EXPECT_EQ(kConfirmationMessageId, actual_sync_consent.confirmation_grd_id());
}

TEST_F(ConsentAuditorImplTest, RecordArcPlayConsentRevocation) {
  auto wrapped_fake_bridge = std::make_unique<FakeConsentSyncBridge>();
  FakeConsentSyncBridge* fake_bridge = wrapped_fake_bridge.get();
  base::SimpleTestClock test_clock;

  SetConsentSyncBridge(std::move(wrapped_fake_bridge));
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetClock(&test_clock);
  BuildConsentAuditorImpl();

  std::vector<int> kDescriptionMessageIds = {12, 37, 42};
  int kConfirmationMessageId = 47;

  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  test_clock.SetNow(now);

  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_status(UserConsentTypes::NOT_GIVEN);
  play_consent.set_confirmation_grd_id(kConfirmationMessageId);
  for (int id : kDescriptionMessageIds) {
    play_consent.add_description_grd_ids(id);
  }
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETTING_CHANGE);
  consent_auditor()->RecordArcPlayConsent(kAccountId, play_consent);

  std::vector<UserConsentSpecifics> consents =
      fake_bridge->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  UserConsentSpecifics consent = consents[0];

  EXPECT_EQ(kAccountId.ToString(), consent.account_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());

  EXPECT_TRUE(consent.has_arc_play_terms_of_service_consent());
  const ArcPlayTermsOfServiceConsent& actual_play_consent =
      consent.arc_play_terms_of_service_consent();
  EXPECT_EQ(UserConsentTypes::NOT_GIVEN, actual_play_consent.status());
  EXPECT_EQ(ArcPlayTermsOfServiceConsent::SETTING_CHANGE,
            actual_play_consent.consent_flow());
  EXPECT_EQ(3, actual_play_consent.description_grd_ids_size());
  EXPECT_EQ(kDescriptionMessageIds[0],
            actual_play_consent.description_grd_ids(0));
  EXPECT_EQ(kDescriptionMessageIds[1],
            actual_play_consent.description_grd_ids(1));
  EXPECT_EQ(kDescriptionMessageIds[2],
            actual_play_consent.description_grd_ids(2));
  EXPECT_EQ(kConfirmationMessageId, actual_play_consent.confirmation_grd_id());
}

TEST_F(ConsentAuditorImplTest, RecordArcPlayConsent) {
  auto wrapped_fake_bridge = std::make_unique<FakeConsentSyncBridge>();
  FakeConsentSyncBridge* fake_bridge = wrapped_fake_bridge.get();
  base::SimpleTestClock test_clock;

  SetConsentSyncBridge(std::move(wrapped_fake_bridge));
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetClock(&test_clock);
  BuildConsentAuditorImpl();

  int kConfirmationMessageId = 47;

  base::Time now;
  ASSERT_TRUE(base::Time::FromUTCString("2017-11-14T15:15:38Z", &now));
  test_clock.SetNow(now);

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

  std::vector<UserConsentSpecifics> consents =
      fake_bridge->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  UserConsentSpecifics consent = consents[0];

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

  syncer::FakeModelTypeControllerDelegate fake_delegate(
      syncer::ModelType::USER_CONSENTS);
  auto expected_delegate_ptr = fake_delegate.GetWeakPtr();
  DCHECK(expected_delegate_ptr);
  fake_bridge->SetControllerDelegate(expected_delegate_ptr);

  SetConsentSyncBridge(std::move(fake_bridge));
  BuildConsentAuditorImpl();

  // There is a bridge (i.e. separate sync type for consents is enabled), thus,
  // there should be a delegate as well.
  EXPECT_EQ(expected_delegate_ptr.get(),
            consent_auditor()->GetControllerDelegate().get());
}

TEST_F(ConsentAuditorImplTest, RecordAssistantActivityControlConsent) {
  auto wrapped_fake_bridge = std::make_unique<FakeConsentSyncBridge>();
  FakeConsentSyncBridge* fake_bridge = wrapped_fake_bridge.get();
  base::SimpleTestClock test_clock;

  SetConsentSyncBridge(std::move(wrapped_fake_bridge));
  SetAppVersion(kCurrentAppVersion);
  SetAppLocale(kCurrentAppLocale);
  SetClock(&test_clock);
  BuildConsentAuditorImpl();

  AssistantActivityControlConsent assistant_consent;
  assistant_consent.set_status(UserConsentTypes::GIVEN);

  const char ui_audit_key[] = {0x67, 0x23, 0x78};
  assistant_consent.set_ui_audit_key(std::string(ui_audit_key, 3));

  assistant_consent.set_setting_type(AssistantActivityControlConsent::ALL);

  consent_auditor()->RecordAssistantActivityControlConsent(kAccountId,
                                                           assistant_consent);

  std::vector<UserConsentSpecifics> consents =
      fake_bridge->GetRecordedUserConsents();
  ASSERT_EQ(1U, consents.size());
  UserConsentSpecifics consent = consents[0];

  EXPECT_EQ(kAccountId.ToString(), consent.account_id());
  EXPECT_EQ(kCurrentAppLocale, consent.locale());

  EXPECT_EQ(true, consent.has_assistant_activity_control_consent());
  EXPECT_EQ(UserConsentTypes::GIVEN,
            consent.assistant_activity_control_consent().status());
  EXPECT_EQ(std::string(ui_audit_key, 3),
            consent.assistant_activity_control_consent().ui_audit_key());
  EXPECT_EQ(AssistantActivityControlConsent::ALL,
            consent.assistant_activity_control_consent().setting_type());
}

}  // namespace consent_auditor
