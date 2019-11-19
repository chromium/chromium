// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_
#define COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/consent_auditor/consent_auditor.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Matcher;

namespace consent_auditor {
// TODO(markusheintz): Rename to MockConsentAuditor
class FakeConsentAuditor : public ConsentAuditor {
 public:
  FakeConsentAuditor();
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

  void RecordLocalConsent(const std::string& feature,
                          const std::string& description_text,
                          const std::string& confirmation_text) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override;

  // Methods for fake.
  // TODO(markusheintz): Replace the usage of this methods in all tests.
  void RecordGaiaConsent(const CoreAccountId& account_id,
                         consent_auditor::Feature feature,
                         const std::vector<int>& description_grd_ids,
                         int confirmation_grd_id,
                         consent_auditor::ConsentStatus status);

  const CoreAccountId& account_id() const { return account_id_; }

  const sync_pb::UserConsentTypes::SyncConsent& recorded_sync_consent() const {
    return recorded_sync_consent_;
  }

  const sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent&
  recorded_play_consent() const {
    return recorded_play_consent_;
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

  sync_pb::UserConsentTypes::SyncConsent recorded_sync_consent_;
  sync_pb::UserConsentTypes_ArcPlayTermsOfServiceConsent recorded_play_consent_;

  std::vector<std::vector<int>> recorded_id_vectors_;
  std::vector<int> recorded_confirmation_ids_;
  std::vector<Feature> recorded_features_;
  std::vector<ConsentStatus> recorded_statuses_;

  DISALLOW_COPY_AND_ASSIGN(FakeConsentAuditor);
};

MATCHER_P(ArcPlayConsentEq, expected_consent, "") {
  const sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent&
      actual_consent = arg;

  if (actual_consent.SerializeAsString() ==
      expected_consent.SerializeAsString())
    return true;

  LOG(ERROR) << "ERROR: actual proto does not match the expected proto";
  return false;
}

MATCHER_P(ArcGoogleLocationServiceConsentEq, expected_consent, "") {
  const sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent&
      actual_consent = arg;

  if (actual_consent.SerializeAsString() ==
      expected_consent.SerializeAsString())
    return true;

  LOG(ERROR) << "ERROR: actual proto does not match the expected proto";
  return false;
}

MATCHER_P(ArcBackupAndRestoreConsentEq, expected_consent, "") {
  if (arg.SerializeAsString() == expected_consent.SerializeAsString())
    return true;

  LOG(ERROR) << "ERROR: actual proto does not match the expected proto";
  return false;
}

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_FAKE_CONSENT_AUDITOR_H_
