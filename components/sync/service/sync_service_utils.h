// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_UTILS_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_UTILS_H_

#include "build/build_config.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/trusted_vault/trusted_vault_client.h"

namespace syncer {

// Indicates whether uploading of data to Google is enabled, i.e. the user has
// given consent to upload this data. Since this enum is used for logging
// histograms, entries must not be removed or reordered.
enum class UploadState {
  // Syncing is enabled in principle, but the sync service is not actually
  // active yet. This is because initialization hasn't completed and hence the
  // auth error state isn't known yet.
  INITIALIZING,
  // We are not syncing to Google, and the caller should assume that we do not
  // have consent to do so. This can have a number of reasons, e.g.: sync as a
  // whole is disabled, or the given data type is disabled, or we're in
  // "local sync" mode, or this data type is encrypted with a custom passphrase
  // (in which case we're technically still uploading, but Google can't inspect
  // the data), or we're in a persistent auth error state. As one special case
  // of an auth error, sync may be "paused" because the user signed out of the
  // content area.
  NOT_ACTIVE,
  // We're actively syncing data to Google servers, in a form that is readable
  // by Google.
  ACTIVE,
  // Used when logging histograms. Must have this exact name.
  kMaxValue = ACTIVE
};

// Returns whether `type` is being uploaded to Google. This is useful for
// features that depend on user consent for uploading data (e.g. history) to
// Google.
UploadState GetUploadToGoogleState(const SyncService* sync_service,
                                   DataType type);

void RecordKeyRetrievalTrigger(
    trusted_vault::TrustedVaultUserActionTriggerForUMA trigger);

void RecordRecoverabilityDegradedFixTrigger(
    trusted_vault::TrustedVaultUserActionTriggerForUMA trigger);

// Whether the user should be offered to opt in to trusted vault encryption.
bool ShouldOfferTrustedVaultOptIn(const SyncService* service);

#if BUILDFLAG(IS_IOS)
// Indicates the UI surface where an identity error is displayed to the user.
// Since this enum is used for logging histograms, entries must not be removed
// or reordered.
// LINT.IfChange(IdentityErrorDisplaySurface)
enum class IdentityErrorDisplaySurface {
  // Displayed in the account menu error card.
  kAccountMenu = 0,
  // Displayed in the sync / account settings page.
  kSyncSettings = 1,
  kMaxValue = kSyncSettings,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:IdentityErrorDisplaySurface)

// Records the surface where an identity error is displayed to the user.
void MaybeRecordIdentityErrorShown(IdentityErrorDisplaySurface surface,
                                   SyncService::UserActionableError error);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_SERVICE_UTILS_H_
