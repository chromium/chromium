// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_
#define COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_

#include <string>
#include <vector>

#include "components/consent_auditor/consent_auditor.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Matcher;

namespace consent_auditor {
// TODO(markusheintz): Rename to MockConsentAuditor
class FakeConsentAuditor : public ConsentAuditor {
 public:
  FakeConsentAuditor();

  FakeConsentAuditor(const FakeConsentAuditor&) = delete;
  FakeConsentAuditor& operator=(const FakeConsentAuditor&) = delete;

  ~FakeConsentAuditor() override;

  // ConsentAuditor implementation.
  void RecordSyncConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::SyncConsent& consent) override;
  MOCK_METHOD2(
      RecordArcPlayConsent,
      void(const CoreAccountId&,
           const sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent&));
  MOCK_METHOD2(
      RecordArcBackupAndRestoreConsent,
      void(const CoreAccountId&,
           const sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent&));
  MOCK_METHOD2(
      RecordArcGoogleLocationServiceConsent,
      void(const CoreAccountId&,
           const sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent&));
  void RecordAssistantActivityControlConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::AssistantActivityControlConsent& consent)
      override;
  void RecordAccountPasswordsConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::AccountPasswordsConsent& consent)
      override;
  MOCK_METHOD2(
      RecordRecorderSpeakerLabelConsent,
      void(const CoreAccountId&,
           const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent&));

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

  // Methods for fake.
  // TODO(markusheintz): Replace the usage of this methods in all tests.
  void RecordGaiaConsent(const CoreAccountId& account_id,
                         consent_auditor::Feature feature,
                         const std::vector<int>& description_grd_ids,
                         int confirmation_grd_id,
                         consent_auditor::ConsentStatus status);

  const CoreAccountId& account_id() const { return account_id_; }

  const std::vector<sync_pb::UserConsentSpecifics>& recorded_consents() const {
    return recorded_consents_;
  }

  const std::vector<std::vector<int>>& recorded_id_vectors() {
    return recorded_id_vectors_;
  }

  const std::vector<int>& recorded_confirmation_ids() const {
    return recorded_confirmation_ids_;
  }

  const std::vector<Feature>& recorded_features() { return recorded_features_; }

  const std::vector<ConsentStatus>& recorded_statuses() {
    return recorded_statuses_;
  }

 private:
  CoreAccountId account_id_;

  // Holds specific consent information for assistant activity control consent
  // and account password consent. Does not (yet) contain recorded sync consent.
  std::vector<sync_pb::UserConsentSpecifics> recorded_consents_;

  std::vector<std::vector<int>> recorded_id_vectors_;
  std::vector<int> recorded_confirmation_ids_;
  std::vector<Feature> recorded_features_;
  std::vector<ConsentStatus> recorded_statuses_;
};

MATCHER_P(ArcPlayConsentEq, expected_consent, "") {
  const sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent&
      actual_consent = arg;

  if (actual_consent.SerializeAsString() ==
      expected_consent.SerializeAsString())
    return true;

  *result_listener << "ERROR: actual proto does not match the expected proto";
  return false;
}

MATCHER_P(ArcGoogleLocationServiceConsentEq, expected_consent, "") {
  const sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent&
      actual_consent = arg;

  if (actual_consent.SerializeAsString() ==
      expected_consent.SerializeAsString())
    return true;

  *result_listener << "ERROR: actual proto does not match the expected proto";
  return false;
}

MATCHER_P(ArcBackupAndRestoreConsentEq, expected_consent, "") {
  if (arg.SerializeAsString() == expected_consent.SerializeAsString())
    return true;

  *result_listener << "ERROR: actual proto does not match the expected proto";
  return false;
}

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_
