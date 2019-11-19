// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_service_utils.h"

#include "base/metrics/histogram_macros.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace syncer {

UploadState GetUploadToGoogleState(const SyncService* sync_service,
                                   ModelType type) {
  // Note: Before configuration is done, GetPreferredDataTypes returns
  // "everything" (i.e. the default setting). If a data type is missing there,
  // it must be because the user explicitly disabled it.
  if (!sync_service || sync_service->IsLocalSyncEnabled() ||
      !sync_service->CanSyncFeatureStart() ||
      !sync_service->GetPreferredDataTypes().Has(type)) {
    return UploadState::NOT_ACTIVE;
  }

  // If the given ModelType is encrypted with a custom passphrase, we also
  // consider uploading inactive, since Google can't read the data.
  // Note that encryption is tricky: Some data types (e.g. PASSWORDS) are always
  // encrypted, but not necessarily with a custom passphrase. On the other hand,
  // some data types are never encrypted (e.g. DEVICE_INFO), even if the
  // "encrypt everything" setting is enabled.
  if (sync_service->GetUserSettings()->GetEncryptedDataTypes().Has(type) &&
      sync_service->GetUserSettings()->IsUsingSecondaryPassphrase()) {
    return UploadState::NOT_ACTIVE;
  }

  // Persistent auth errors always map to NOT_ACTIVE. For transient errors, we
  // give the benefit of the doubt and may still say we're INITIALIZING.
  if (sync_service->GetAuthError().IsPersistentError()) {
    return UploadState::NOT_ACTIVE;
  }

  switch (sync_service->GetTransportState()) {
    case SyncService::TransportState::DISABLED:
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
      if (sync_service->GetAuthError().IsTransientError()) {
        return UploadState::INITIALIZING;
      }
      // TODO(crbug.com/831579): We only know if the refresh token is actually
      // valid (no auth error) after we've tried talking to the Sync server.
      if (!sync_service->HasCompletedSyncCycle()) {
        return UploadState::INITIALIZING;
      }
      return UploadState::ACTIVE;
  }
  NOTREACHED();
  return UploadState::NOT_ACTIVE;
}

void RecordSyncEvent(SyncEventCodes code) {
  UMA_HISTOGRAM_ENUMERATION("Sync.EventCodes", code, MAX_SYNC_EVENT_CODE);
}

}  // namespace syncer
