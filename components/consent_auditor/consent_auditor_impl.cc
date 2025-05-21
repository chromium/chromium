// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_auditor_impl.h"

#include <memory>
#include <utility>

#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/protocol/user_consent_types.pb.h"

using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using sync_pb::UserConsentSpecifics;
using sync_pb::UserConsentTypes;

namespace consent_auditor {

namespace {

std::unique_ptr<sync_pb::UserConsentSpecifics> CreateUserConsentSpecifics(
    const GaiaId& gaia_id,
    const std::string& locale,
    base::Clock* clock) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      std::make_unique<sync_pb::UserConsentSpecifics>();
  specifics->set_obfuscated_gaia_id(gaia_id.ToString());
  specifics->set_client_consent_time_usec(
      clock->Now().since_origin().InMicroseconds());
  specifics->set_locale(locale);

  return specifics;
}

}  // namespace

ConsentAuditorImpl::ConsentAuditorImpl(
    std::unique_ptr<ConsentSyncBridge> consent_sync_bridge,
    const std::string& app_locale,
    base::Clock* clock)
    : consent_sync_bridge_(std::move(consent_sync_bridge)),
      app_locale_(app_locale),
      clock_(clock) {
  DCHECK(consent_sync_bridge_);
}

ConsentAuditorImpl::~ConsentAuditorImpl() = default;

void ConsentAuditorImpl::Shutdown() {}

void ConsentAuditorImpl::RecordArcPlayConsent(
    const GaiaId& gaia_id,
    const ArcPlayTermsOfServiceConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(gaia_id, app_locale_, clock_);

  sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent* arc_play_consent =
      specifics->mutable_arc_play_terms_of_service_consent();
  arc_play_consent->CopyFrom(consent);
  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordArcGoogleLocationServiceConsent(
    const GaiaId& gaia_id,
    const UserConsentTypes::ArcGoogleLocationServiceConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(gaia_id, app_locale_, clock_);

  sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent*
      arc_google_location_service_consent =
          specifics->mutable_arc_location_service_consent();
  arc_google_location_service_consent->CopyFrom(consent);
  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordArcBackupAndRestoreConsent(
    const GaiaId& gaia_id,
    const UserConsentTypes::ArcBackupAndRestoreConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(gaia_id, app_locale_, clock_);

  sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent*
      arc_backup_and_restore_consent =
          specifics->mutable_arc_backup_and_restore_consent();
  arc_backup_and_restore_consent->CopyFrom(consent);
  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordSyncConsent(
    const GaiaId& gaia_id,
    const UserConsentTypes::SyncConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(gaia_id, app_locale_, clock_);

  sync_pb::UserConsentTypes::SyncConsent* sync_consent =
      specifics->mutable_sync_consent();
  sync_consent->CopyFrom(consent);
  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordAssistantActivityControlConsent(
    const GaiaId& gaia_id,
    const sync_pb::UserConsentTypes::AssistantActivityControlConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(gaia_id, app_locale_, clock_);
  sync_pb::UserConsentTypes::AssistantActivityControlConsent*
      assistant_consent =
          specifics->mutable_assistant_activity_control_consent();
  assistant_consent->CopyFrom(consent);

  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordAccountPasswordsConsent(
    const GaiaId& gaia_id,
    const sync_pb::UserConsentTypes::AccountPasswordsConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(gaia_id, app_locale_, clock_);
  specifics->mutable_account_passwords_consent()->CopyFrom(consent);

  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

void ConsentAuditorImpl::RecordRecorderSpeakerLabelConsent(
    const GaiaId& gaia_id,
    const sync_pb::UserConsentTypes::RecorderSpeakerLabelConsent& consent) {
  std::unique_ptr<sync_pb::UserConsentSpecifics> specifics =
      CreateUserConsentSpecifics(gaia_id, app_locale_, clock_);
  specifics->mutable_recorder_speaker_label_consent()->CopyFrom(consent);

  consent_sync_bridge_->RecordConsent(std::move(specifics));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
ConsentAuditorImpl::GetControllerDelegate() {
  if (consent_sync_bridge_) {
    return consent_sync_bridge_->GetControllerDelegate();
  }
  return base::WeakPtr<syncer::DataTypeControllerDelegate>();
}

}  // namespace consent_auditor
