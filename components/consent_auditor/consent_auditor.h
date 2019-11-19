// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_type_sync_bridge.h"
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

  FEATURE_LAST = ASSISTANT_ACTIVITY_CONTROL
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

  // Records that the user consented to a |feature|. The user was presented with
  // |description_text| and accepted it by interacting |confirmation_text|
  // (e.g. clicking on a button; empty if not applicable).
  // Returns true if successful.
  virtual void RecordLocalConsent(const std::string& feature,
                                  const std::string& description_text,
                                  const std::string& confirmation_text) = 0;

  // Returns the underlying Sync integration point.
  virtual base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegate() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConsentAuditor);
};

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_
