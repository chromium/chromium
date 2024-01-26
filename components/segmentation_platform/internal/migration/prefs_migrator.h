// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_PREFS_MIGRATOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_PREFS_MIGRATOR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/result.h"

class PrefService;

namespace segmentation_platform {
struct Config;

// PrefsMigrator migrates pref entries for models that support multi output from
// old prefs to new prefs.
class PrefsMigrator {
 public:
  PrefsMigrator(PrefService* pref_service,
                ClientResultPrefs* client_prefs,
                const std::vector<std::unique_ptr<Config>>& configs);

  ~PrefsMigrator();

  // Disallow copy/assign.
  PrefsMigrator(PrefsMigrator&) = delete;
  PrefsMigrator& operator=(PrefsMigrator&) = delete;

  // An entry for model result is migrated from old prefs to new prefs if:
  // 1. New pref doesn't have model result entry, whereas old one does have an
  //    entry.
  // 2. Old prefs is updated after writing to new prefs.
  // Migration involves converting `SegmentResult` from old_prefs to
  // `ClientResult` and saving it to new prefs. It will also delete the entries
  // from old prefs for models migrated to support multi output.
  void MigrateOldPrefsToNewPrefs();

 private:
  bool IsPrefMigrationRequired(Config* config);
  void UpdateNewPrefs(Config* config,
                      std::optional<SelectedSegment> old_result,
                      const proto::ClientResult* new_result);
  void DeleteOldPrefsEntryIfFullyMigrated(
      Config* config,
      std::optional<SelectedSegment> old_result);

  // Configs for all registered clients.
  const raw_ref<const std::vector<std::unique_ptr<Config>>> configs_;

  // The underlying pref backed store to read the pref values from before multi
  // output.
  std::unique_ptr<SegmentationResultPrefs> old_prefs_;

  // The underlying pref backed store to read the pref values from after multi
  // output support.
  const raw_ptr<ClientResultPrefs> new_prefs_;

  base::WeakPtrFactory<PrefsMigrator> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_MIGRATION_PREFS_MIGRATOR_H_
