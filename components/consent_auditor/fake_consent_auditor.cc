// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "components/consent_auditor/consent_auditor.h"
#include "components/consent_auditor/fake_consent_auditor.h"

namespace {

consent_auditor::ConsentStatus ConvertConsentStatus(
    sync_pb::UserConsentTypes::ConsentStatus consent_status) {
  DCHECK_NE(consent_status,
            sync_pb::UserConsentTypes::ConsentStatus::
                UserConsentTypes_ConsentStatus_CONSENT_STATUS_UNSPECIFIED);

  if (consent_status == sync_pb::UserConsentTypes::ConsentStatus::
                            UserConsentTypes_ConsentStatus_GIVEN) {
    return consent_auditor::ConsentStatus::GIVEN;
  }
  return consent_auditor::ConsentStatus::NOT_GIVEN;
}

}  // namespace

namespace consent_auditor {

FakeConsentAuditor::FakeConsentAuditor() {}

FakeConsentAuditor::~FakeConsentAuditor() {}

void FakeConsentAuditor::RecordSyncConsent(
    const CoreAccountId& account_id,
    const sync_pb::UserConsentTypes::SyncConsent& consent) {
  // TODO(markusheintz): Change the Fake to store the proto instead of calling
  // RecordGaiaConsent.
  std::vector<int> description_grd_ids(consent.description_grd_ids().begin(),
                                       consent.description_grd_ids().end());
  RecordGaiaConsent(account_id, Feature::CHROME_SYNC, description_grd_ids,
                    consent.confirmation_grd_id(),
                    ConvertConsentStatus(consent.status()));
}

void FakeConsentAuditor::RecordAssistantActivityControlConsent(
    const CoreAccountId& account_id,
    const sync_pb::UserConsentTypes::AssistantActivityControlConsent& consent) {
  NOTIMPLEMENTED();
}

void FakeConsentAuditor::RecordGaiaConsent(
    const CoreAccountId& account_id,
    consent_auditor::Feature feature,
    const std::vector<int>& description_grd_ids,
    int confirmation_grd_id,
    consent_auditor::ConsentStatus status) {
  account_id_ = account_id;
  recorded_id_vectors_.push_back(description_grd_ids);
  recorded_confirmation_ids_.push_back(confirmation_grd_id);
  recorded_features_.push_back(feature);
  recorded_statuses_.push_back(status);
}

void FakeConsentAuditor::RecordLocalConsent(
    const std::string& feature,
    const std::string& description_text,
    const std::string& confirmation_text) {
  NOTIMPLEMENTED();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
FakeConsentAuditor::GetControllerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace consent_auditor
