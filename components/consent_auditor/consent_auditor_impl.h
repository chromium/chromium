// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_IMPL_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/consent_auditor/consent_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace sync_pb {
class UserConsentSpecifics;
}  // namespace sync_pb

class PrefService;
class PrefRegistrySimple;

namespace consent_auditor {

class ConsentAuditorImpl : public ConsentAuditor {
 public:
  ConsentAuditorImpl(PrefService* pref_service,
                     std::unique_ptr<ConsentSyncBridge> consent_sync_bridge,
                     const std::string& app_version,
                     const std::string& app_locale,
                     base::Clock* clock);
  ~ConsentAuditorImpl() override;

  // KeyedService (through ConsentAuditor) implementation.
  void Shutdown() override;

  // Registers the preferences needed by this service.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void RecordArcPlayConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent& consent)
      override;
  void RecordArcGoogleLocationServiceConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent& consent)
      override;
  void RecordArcBackupAndRestoreConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent& consent)
      override;
  void RecordSyncConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::SyncConsent& consent) override;
  void RecordAssistantActivityControlConsent(
      const CoreAccountId& account_id,
      const sync_pb::UserConsentTypes::AssistantActivityControlConsent& consent)
      override;
  void RecordLocalConsent(const std::string& feature,
                          const std::string& description_text,
                          const std::string& confirmation_text) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate()
      override;

 private:
  std::unique_ptr<sync_pb::UserConsentSpecifics> ConstructUserConsentSpecifics(
      const CoreAccountId& account_id,
      Feature feature,
      const std::vector<int>& description_grd_ids,
      int confirmation_grd_id,
      ConsentStatus status);

  PrefService* pref_service_;
  std::unique_ptr<ConsentSyncBridge> consent_sync_bridge_;
  std::string app_version_;
  std::string app_locale_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(ConsentAuditorImpl);
};

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_IMPL_H_
