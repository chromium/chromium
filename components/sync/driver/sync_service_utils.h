// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_

#include "components/sync/base/model_type.h"

namespace syncer {

class SyncService;

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
  // whole is disabled, or the given model type is disabled, or we're in
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

// Used for UMA histogram, do not reorder. Represents the UI elements which
// contain trusted vault error button.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.sync
enum class TrustedVaultUserActionTriggerForUMA {
  // Settings pages, used on all platforms except ChromeOS.
  kSettings,
  // Used on desktop platform only.
  kProfileMenu,
  // Used on Android and ChromeOS, represents OS-level notification.
  kNotification,
  // Used on iOS only. Represents Infobar on the New Tab Page.
  // TODO(crbug.com/1083973): record this bucket bucket on Android once
  // corresponding UI added.
  kNewTabPageInfobar,
  kMaxValue = kNewTabPageInfobar
};

// Returns whether |type| is being uploaded to Google. This is useful for
// features that depend on user consent for uploading data (e.g. history) to
// Google.
UploadState GetUploadToGoogleState(const SyncService* sync_service,
                                   ModelType type);

void RecordKeyRetrievalTrigger(TrustedVaultUserActionTriggerForUMA trigger);

void RecordRecoverabilityDegradedFixTrigger(
    TrustedVaultUserActionTriggerForUMA trigger);

// Whether the user should be offered to opt in to trusted vault encryption.
bool ShouldOfferTrustedVaultOptIn(const SyncService* service);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_SERVICE_UTILS_H_
