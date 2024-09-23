// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_service_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace syncer {

UploadState GetUploadToGoogleState(const SyncService* sync_service,
                                   DataType type) {
  // Without a SyncService, or in local-sync mode, there's no upload to Google.
  if (!sync_service || sync_service->IsLocalSyncEnabled()) {
    return UploadState::NOT_ACTIVE;
  }

  // Make sure that the current user settings include `type`.
  if (!sync_service->GetPreferredDataTypes().Has(type)) {
    return UploadState::NOT_ACTIVE;
  }

  // If the given DataType is encrypted with a custom passphrase, we also
  // consider uploading inactive, since Google can't read the data.
  // Note that encryption is tricky: Some data types (e.g. PASSWORDS) are always
  // encrypted, but not necessarily with a custom passphrase. On the other hand,
  // some data types are never encrypted (e.g. DEVICE_INFO), even if the
  // "encrypt everything" setting is enabled.
  if (sync_service->GetUserSettings()->GetAllEncryptedDataTypes().Has(type) &&
      sync_service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    return UploadState::NOT_ACTIVE;
  }

  // Persistent auth errors always map to NOT_ACTIVE because the transport is
  // guaranteed to be PAUSED.
  if (sync_service->GetAuthError().IsPersistentError()) {
    DCHECK_EQ(sync_service->GetTransportState(),
              SyncService::TransportState::PAUSED);
  }

  // SyncService never reports transient errors.
  DCHECK(!sync_service->GetAuthError().IsTransientError());

  switch (sync_service->GetTransportState()) {
    case SyncService::TransportState::DISABLED:
    case SyncService::TransportState::PAUSED:
      return UploadState::NOT_ACTIVE;

    case SyncService::TransportState::START_DEFERRED:
    case SyncService::TransportState::INITIALIZING:
    case SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case SyncService::TransportState::CONFIGURING:
      return UploadState::INITIALIZING;

    case SyncService::TransportState::ACTIVE:
      // If sync is active, but the data type in question still isn't, then
      // something must have gone wrong with that data type.
      if (!sync_service->GetActiveDataTypes().Has(type)) {
        return UploadState::NOT_ACTIVE;
      }
      // TODO(crbug.com/41382444): We only know if the refresh token is actually
      // valid (no auth error) after we've tried talking to the Sync server.
      if (!sync_service->HasCompletedSyncCycle()) {
        return UploadState::INITIALIZING;
      }
      return UploadState::ACTIVE;
  }
  NOTREACHED_IN_MIGRATION();
  return UploadState::NOT_ACTIVE;
}

void RecordKeyRetrievalTrigger(TrustedVaultUserActionTriggerForUMA trigger) {
  base::UmaHistogramEnumeration("Sync.TrustedVaultKeyRetrievalTrigger",
                                trigger);
}

void RecordRecoverabilityDegradedFixTrigger(
    TrustedVaultUserActionTriggerForUMA trigger) {
  base::UmaHistogramEnumeration(
      "Sync.TrustedVaultRecoverabilityDegradedFixTrigger", trigger);
}

bool ShouldOfferTrustedVaultOptIn(const SyncService* service) {
  if (!service) {
    return false;
  }

  if (service->GetTransportState() != SyncService::TransportState::ACTIVE) {
    // Transport state must be active so SyncUserSettings::GetPassphraseType()
    // changes once the opt-in completes, and the UI is notified.
    return false;
  }

  const DataTypeSet encrypted_types =
      service->GetUserSettings()->GetAllEncryptedDataTypes();
  if (Intersection(service->GetActiveDataTypes(), encrypted_types).empty()) {
    // No point in offering the user a new encryption method if they are not
    // syncing any encrypted types.
    return false;
  }

  switch (service->GetUserSettings()->GetPassphraseType().value_or(
      PassphraseType::kImplicitPassphrase)) {
    case PassphraseType::kImplicitPassphrase:
    case PassphraseType::kFrozenImplicitPassphrase:
    case PassphraseType::kCustomPassphrase:
    case PassphraseType::kTrustedVaultPassphrase:
      // Either trusted vault is already set or a transition from this
      // passphrase type to trusted vault is disallowed.
      return false;
    case PassphraseType::kKeystorePassphrase:
      // Passphrase required should be extremely rare.
      return !service->GetUserSettings()->IsPassphraseRequired();
  }
}

}  // namespace syncer
