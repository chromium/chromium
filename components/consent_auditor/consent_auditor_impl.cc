// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_auditor_impl.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "components/consent_auditor/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/model/model_type_sync_bridge.h"

using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using sync_pb::UserConsentTypes;
using sync_pb::UserConsentSpecifics;

namespace consent_auditor {

namespace {

const char kLocalConsentDescriptionKey[] = "description";
const char kLocalConsentConfirmationKey[] = "confirmation";
const char kLocalConsentVersionKey[] = "version";
const char kLocalConsentLocaleKey[] = "locale";

std::unique_ptr<sync_pb::UserConsentSpecifics> CreateUserConsentSpecifics(
    const CoreAccountId& account_id,
    const std::string& locale,
    base::Clock* clock) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      std::make_unique<sync_pb::UserConsentSpecifics>();
  specifics->set_account_id(account_id.ToString());
  specifics->set_client_consent_time_usec(
      clock->Now().since_origin().InMicroseconds());
  specifics->set_locale(locale);

  return specifics;
}

}  // namespace

ConsentAuditorImpl::ConsentAuditorImpl(
    PrefService* pref_service,
    std::unique_ptr<ConsentSyncBridge> consent_sync_bridge,
    const std::string& app_version,
    const std::string& app_locale,
    base::Clock* clock)
    : pref_service_(pref_service),
      consent_sync_bridge_(std::move(consent_sync_bridge)),
      app_version_(app_version),
      app_locale_(app_locale),
      clock_(clock) {
  DCHECK(consent_sync_bridge_);
  DCHECK(pref_service_);
}

ConsentAuditorImpl::~ConsentAuditorImpl() {}

void ConsentAuditorImpl::Shutdown() {}

// static
void ConsentAuditorImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kLocalConsentsDictionary);
}

void ConsentAuditorImpl::RecordArcPlayConsent(
    const CoreAccountId& account_id,
    const ArcPlayTermsOfServiceConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(account_id, app_locale_, clock_);

  sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent* arc_play_consent =
      specifics->mutable_arc_play_terms_of_service_consent();
  arc_play_consent->CopyFrom(consent);
  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordArcGoogleLocationServiceConsent(
    const CoreAccountId& account_id,
    const UserConsentTypes::ArcGoogleLocationServiceConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(account_id, app_locale_, clock_);

  sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent*
      arc_google_location_service_consent =
          specifics->mutable_arc_location_service_consent();
  arc_google_location_service_consent->CopyFrom(consent);
  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordArcBackupAndRestoreConsent(
    const CoreAccountId& account_id,
    const UserConsentTypes::ArcBackupAndRestoreConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(account_id, app_locale_, clock_);

  sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent*
      arc_backup_and_restore_consent =
          specifics->mutable_arc_backup_and_restore_consent();
  arc_backup_and_restore_consent->CopyFrom(consent);
  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordSyncConsent(
    const CoreAccountId& account_id,
    const UserConsentTypes::SyncConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(account_id, app_locale_, clock_);

  sync_pb::UserConsentTypes::SyncConsent* sync_consent =
      specifics->mutable_sync_consent();
  sync_consent->CopyFrom(consent);
  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordAssistantActivityControlConsent(
    const CoreAccountId& account_id,
    const sync_pb::UserConsentTypes::AssistantActivityControlConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(account_id, app_locale_, clock_);
  sync_pb::UserConsentTypes::AssistantActivityControlConsent*
      assistant_consent =
          specifics->mutable_assistant_activity_control_consent();
  assistant_consent->CopyFrom(consent);

  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordAccountPasswordsConsent(
    const CoreAccountId& account_id,
    const sync_pb::UserConsentTypes::AccountPasswordsConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(account_id, app_locale_, clock_);
  specifics->mutable_account_passwords_consent()->CopyFrom(consent);

  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordLocalConsent(
    const std::string& feature,
    const std::string& description_text,
    const std::string& confirmation_text) {
  DictionaryPrefUpdate consents_update(pref_service_,
                                       prefs::kLocalConsentsDictionary);
  base::DictionaryValue* consents = consents_update.Get();
  DCHECK(consents);

  base::DictionaryValue record;
  record.SetKey(kLocalConsentDescriptionKey, base::Value(description_text));
  record.SetKey(kLocalConsentConfirmationKey, base::Value(confirmation_text));
  record.SetKey(kLocalConsentVersionKey, base::Value(app_version_));
  record.SetKey(kLocalConsentLocaleKey, base::Value(app_locale_));

  consents->SetKey(feature, std::move(record));
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ConsentAuditorImpl::GetControllerDelegate() {
  if (consent_sync_bridge_) {
    return consent_sync_bridge_->GetControllerDelegate();
  }
  return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
}

}  // namespace consent_auditor
