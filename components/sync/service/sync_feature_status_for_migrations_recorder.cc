// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_feature_status_for_migrations_recorder.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_service.h"

namespace syncer {

SyncFeatureStatusForSyncToSigninMigration
SyncFeatureStatusForSyncToSigninMigrationFromInt(int value) {
  auto converted =
      static_cast<SyncFeatureStatusForSyncToSigninMigration>(value);
  // Verify that `converted` is actually a valid enum value.
  switch (converted) {
    case SyncFeatureStatusForSyncToSigninMigration::kUndefined:
    case SyncFeatureStatusForSyncToSigninMigration::kDisabled:
    case SyncFeatureStatusForSyncToSigninMigration::kPaused:
    case SyncFeatureStatusForSyncToSigninMigration::kInitializing:
    case SyncFeatureStatusForSyncToSigninMigration::kActive:
      return converted;
  }
  // Unknown/invalid value; fall back to "undefined". In particular the legacy
  // kDisabledOrPaused value will fall here.
  return SyncFeatureStatusForSyncToSigninMigration::kUndefined;
}

SyncFeatureStatusForMigrationsRecorder::SyncFeatureStatusForMigrationsRecorder(
    PrefService* prefs,
    SyncService* sync)
    : prefs_(prefs) {
  // Record metrics on the pre-existing status.
  SyncFeatureStatusForSyncToSigninMigration old_status =
      SyncFeatureStatusForSyncToSigninMigrationFromInt(prefs->GetInteger(
          prefs::internal::kSyncFeatureStatusForSyncToSigninMigration));
  base::UmaHistogramEnumeration("Sync.FeatureStatusForSyncToSigninMigration",
                                old_status);
  if (old_status == SyncFeatureStatusForSyncToSigninMigration::kActive) {
    for (DataType type : ProtocolTypes()) {
      bool type_status = prefs->GetBoolean(GetDataTypeStatusPrefName(type));
      base::UmaHistogramBoolean(
          base::StrCat({"Sync.DataTypeActiveForSyncToSigninMigration.",
                        DataTypeToHistogramSuffix(type)}),
          type_status);
    }
  }

  // Start observing the SyncService, and query the initial state. This ensures
  // that any pre-existing pref values get overridden, and can't carry over into
  // this or future runs of Chrome.
  // TODO(crbug.com/40282890): Find a way to clear the prefs even if this class
  // doesn't even get instantiated, e.g. due to --disable-sync.
  sync->AddObserver(this);
  OnStateChanged(sync);
}

SyncFeatureStatusForMigrationsRecorder::
    ~SyncFeatureStatusForMigrationsRecorder() = default;

// static
void SyncFeatureStatusForMigrationsRecorder::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::internal::kSyncFeatureStatusForSyncToSigninMigration,
      static_cast<int>(SyncFeatureStatusForSyncToSigninMigration::kUndefined));

  for (DataType type : ProtocolTypes()) {
    registry->RegisterBooleanPref(GetDataTypeStatusPrefName(type), false);
  }
}

// static
SyncFeatureStatusForSyncToSigninMigration
SyncFeatureStatusForMigrationsRecorder::
    GetSyncFeatureStatusForSyncToSigninMigration(const PrefService* prefs) {
  return SyncFeatureStatusForSyncToSigninMigrationFromInt(prefs->GetInteger(
      prefs::internal::kSyncFeatureStatusForSyncToSigninMigration));
}

// static
bool SyncFeatureStatusForMigrationsRecorder::
    GetSyncDataTypeActiveForSyncToSigninMigration(const PrefService* prefs,
                                                  DataType type) {
  return prefs->GetBoolean(GetDataTypeStatusPrefName(type));
}

void SyncFeatureStatusForMigrationsRecorder::OnStateChanged(SyncService* sync) {
  // Determine overall Sync-the-feature status and persist it to prefs.
  SyncFeatureStatusForSyncToSigninMigration status =
      DetermineSyncFeatureStatus(sync);
  prefs_->SetInteger(
      prefs::internal::kSyncFeatureStatusForSyncToSigninMigration,
      static_cast<int>(status));

  // Determine per-data-type status (which can only be true if the overall
  // feature is active) and persist to prefs.
  bool feature_is_active =
      (status == SyncFeatureStatusForSyncToSigninMigration::kActive);
  DataTypeSet active_types = sync->GetActiveDataTypes();
  for (DataType type : ProtocolTypes()) {
    prefs_->SetBoolean(GetDataTypeStatusPrefName(type),
                       feature_is_active && active_types.Has(type));
  }
}

void SyncFeatureStatusForMigrationsRecorder::OnSyncShutdown(SyncService* sync) {
  sync->RemoveObserver(this);
}

// static
std::string SyncFeatureStatusForMigrationsRecorder::GetDataTypeStatusPrefName(
    DataType type) {
  return base::StrCat(
      {prefs::internal::kSyncDataTypeStatusForSyncToSigninMigrationPrefix, ".",
       DataTypeToStableLowerCaseString(type)});
}

SyncFeatureStatusForSyncToSigninMigration
SyncFeatureStatusForMigrationsRecorder::DetermineSyncFeatureStatus(
    const SyncService* sync) const {
  if (!sync->IsSyncFeatureEnabled()) {
    return SyncFeatureStatusForSyncToSigninMigration::kDisabled;
  }

  switch (sync->GetTransportState()) {
    case SyncService::TransportState::DISABLED:
      return SyncFeatureStatusForSyncToSigninMigration::kDisabled;
    case SyncService::TransportState::PAUSED:
      return SyncFeatureStatusForSyncToSigninMigration::kPaused;
    case SyncService::TransportState::START_DEFERRED:
    case SyncService::TransportState::INITIALIZING:
    case SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case SyncService::TransportState::CONFIGURING:
      return SyncFeatureStatusForSyncToSigninMigration::kInitializing;
    case SyncService::TransportState::ACTIVE:
      return SyncFeatureStatusForSyncToSigninMigration::kActive;
  }

  NOTREACHED();
}

}  // namespace syncer
