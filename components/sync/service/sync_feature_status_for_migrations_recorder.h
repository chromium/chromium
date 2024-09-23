// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_FEATURE_STATUS_FOR_MIGRATIONS_RECORDER_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_FEATURE_STATUS_FOR_MIGRATIONS_RECORDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace syncer {

class SyncService;

// These values are persisted to logs *and* prefs. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(SyncFeatureStatusForSyncToSigninMigration)
enum class SyncFeatureStatusForSyncToSigninMigration {
  kUndefined = 0,
  // Deprecated: kDisabledOrPaused = 1,
  kInitializing = 2,
  kActive = 3,
  kPaused = 4,
  kDisabled = 5,
  kMaxValue = kDisabled
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncFeatureStatusForSyncToSigninMigration)

// Safely converts an int (e.g. as read from PrefService) back to an enum entry,
// falling back to `kUndefined` if the value doesn't map to any enum entry.
SyncFeatureStatusForSyncToSigninMigration
SyncFeatureStatusForSyncToSigninMigrationFromInt(int value);

// Records prefs related to the status of Sync-the-feature itself as well as
// all data types, which is useful for data migrations that run early during
// browser startup, before any Sync machinery is initialized.
class SyncFeatureStatusForMigrationsRecorder : public SyncServiceObserver {
 public:
  // Both `prefs` and `sync` must be non-null, and must outlive this object.
  SyncFeatureStatusForMigrationsRecorder(PrefService* prefs, SyncService* sync);

  SyncFeatureStatusForMigrationsRecorder(
      const SyncFeatureStatusForMigrationsRecorder&) = delete;
  SyncFeatureStatusForMigrationsRecorder& operator=(
      const SyncFeatureStatusForMigrationsRecorder&) = delete;
  ~SyncFeatureStatusForMigrationsRecorder() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  static SyncFeatureStatusForSyncToSigninMigration
  GetSyncFeatureStatusForSyncToSigninMigration(const PrefService* prefs);

  static bool GetSyncDataTypeActiveForSyncToSigninMigration(
      const PrefService* prefs,
      DataType type);

  // SyncServiceObserver implementation.
  void OnStateChanged(SyncService* sync) override;
  void OnSyncShutdown(SyncService* sync) override;

 private:
  static std::string GetDataTypeStatusPrefName(DataType type);

  SyncFeatureStatusForSyncToSigninMigration DetermineSyncFeatureStatus(
      const SyncService* sync) const;

  raw_ptr<PrefService> prefs_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_FEATURE_STATUS_FOR_MIGRATIONS_RECORDER_H_
