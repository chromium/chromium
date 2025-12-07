// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/fake_consent_auditor.h"

#include <string>
#include <utility>

#include "base/notimplemented.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/sync/protocol/user_consent_specifics.pb.h"
#include "components/sync/protocol/user_consent_types.pb.h"

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

FakeConsentAuditor::FakeConsentAuditor() = default;

FakeConsentAuditor::~FakeConsentAuditor() = default;

void FakeConsentAuditor::RecordSyncConsent(
    const GaiaId& gaia_id,
    const sync_pb::UserConsentTypes::SyncConsent& consent) {
  // TODO(markusheintz): Change the Fake to store the proto instead of calling
  // RecordGaiaConsent.
  std::vector<int> description_grd_ids(consent.description_grd_ids().begin(),
                                       consent.description_grd_ids().end());
  RecordGaiaConsent(gaia_id, Feature::CHROME_SYNC, description_grd_ids,
                    consent.confirmation_grd_id(),
                    ConvertConsentStatus(consent.status()));
}

void FakeConsentAuditor::RecordGaiaConsent(
    const GaiaId& gaia_id,
    consent_auditor::Feature feature,
    const std::vector<int>& description_grd_ids,
    int confirmation_grd_id,
    consent_auditor::ConsentStatus status) {
  gaia_id_ = gaia_id;
  recorded_id_vectors_.push_back(description_grd_ids);
  recorded_confirmation_ids_.push_back(confirmation_grd_id);
  recorded_features_.push_back(feature);
  recorded_statuses_.push_back(status);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FakeConsentAuditor::GetControllerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace consent_auditor
