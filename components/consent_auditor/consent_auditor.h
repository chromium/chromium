// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "google_apis/gaia/core_account_id.h"

namespace consent_auditor {

// Feature for which a consent moment is to be recorded.
//
// This enum is used in histograms. Entries should not be renumbered and
// numeric values should never be reused.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.consent_auditor
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ConsentAuditorFeature
enum class Feature {
  CHROME_SYNC = 0,
  PLAY_STORE = 1,
  BACKUP_AND_RESTORE = 2,
  GOOGLE_LOCATION_SERVICE = 3,
  // CHROME_UNIFIED_CONSENT = 4, (deprecated, not used)
  ASSISTANT_ACTIVITY_CONTROL = 5,
  RECORDER_SPEAKER_LABEL = 6,

  FEATURE_LAST = RECORDER_SPEAKER_LABEL,
};

// Whether a consent is given or not given.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.consent_auditor
enum class ConsentStatus { NOT_GIVEN, GIVEN };

// TODO(markusheintz): Document this class.
class ConsentAuditor : public KeyedService {
 public:
  ConsentAuditor() = default;

  ConsentAuditor(const ConsentAuditor&) = delete;
  ConsentAuditor& operator=(const ConsentAuditor&) = delete;

  ~ConsentAuditor() override = default;

  // Records the ARC Play |consent| for the signed-in GAIA account with the ID
  // |account_id| (as defined in AccountInfo).
  virtual void RecordArcPlayConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent&
          consent) = 0;

  // Records the ARC Google Location Service |consent| for the signed-in GAIA
  // account with the ID |account_id| (as defined in AccountInfo).
  virtual void RecordArcGoogleLocationServiceConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent&
          consent) = 0;

  // Records the ARC Backup and Restore |consent| for the signed-in GAIA
  // account with the ID |account_id| (as defined in AccountInfo).
  virtual void RecordArcBackupAndRestoreConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent& consent) = 0;

  // Records the Sync |consent| for the signed-in GAIA account with the ID
  // |account_id| (as defined in AccountInfo).
  virtual void RecordSyncConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::SyncConsent& consent) = 0;

  // Records the Assistant activity control |consent| for the signed-in GAIA
  // account with the ID |accounts_id| (as defined in Account Info).
  virtual void RecordAssistantActivityControlConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::AssistantActivityControlConsent&
          consent) = 0;

  // Records the Recorder app speaker label |consent| for the signed-in GAIA
  // account with the ID |accounts_id| (as defined in Account Info).
  virtual void RecordRecorderSpeakerLabelConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent&
          consent) = 0;

  // Records the |consent| to download and use passwords from the signed-in GAIA
  // account with the ID |account_id| (as defined in AccountInfo).
  virtual void RecordAccountPasswordsConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::AccountPasswordsConsent& consent) = 0;

  // Returns the underlying Sync integration point.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_
