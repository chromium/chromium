// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/migration/prefs_migrator.h"

#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/internal/migration/result_migration_utils.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {

PrefsMigrator::PrefsMigrator(
    PrefService* pref_service,
    ClientResultPrefs* client_prefs,
    const std::vector<std::unique_ptr<Config>>& configs)
    : configs_(configs), new_prefs_(client_prefs) {
  old_prefs_ = std::make_unique<SegmentationResultPrefs>(pref_service);
}

PrefsMigrator::~PrefsMigrator() = default;

void PrefsMigrator::MigrateOldPrefsToNewPrefs() {
  for (const auto& config : *configs_) {
    if (!IsPrefMigrationRequired(config.get())) {
      continue;
    }

    const auto& segmentation_key = config->segmentation_key;
    std::optional<SelectedSegment> old_result =
        old_prefs_->ReadSegmentationResultFromPref(segmentation_key);
    const proto::ClientResult* new_result =
        new_prefs_->ReadClientResultFromPrefs(segmentation_key);

    UpdateNewPrefs(config.get(), old_result, new_result);
  }
}

bool PrefsMigrator::IsPrefMigrationRequired(Config* config) {
  return (pref_migration_utils::GetClassifierTypeForMigration(
              config->segmentation_key) ==
          proto::Predictor::kBinaryClassifier) ||
         (config->segmentation_key == kAdaptiveToolbarSegmentationKey);
}

void PrefsMigrator::UpdateNewPrefs(Config* config,
                                   std::optional<SelectedSegment> old_result,
                                   const proto::ClientResult* new_result) {
  // Update the new prefs only if:
  // 1. Old pref have entry and new pref doesn't.
  // 2. Old pref was updated, after the new prefs was last written.
  bool old_pref_entry_absent_in_new_pref = !new_result && old_result;
  bool old_pref_entry_updated =
      (new_result && old_result &&
       new_result->timestamp_us() <
           old_result.value()
               .selection_time.ToDeltaSinceWindowsEpoch()
               .InMicroseconds());
  if (old_pref_entry_absent_in_new_pref || old_pref_entry_updated) {
    auto updated_result = pref_migration_utils::CreateClientResultFromOldResult(
        config, old_result.value());
    new_prefs_->SaveClientResultToPrefs(config->segmentation_key,
                                        std::move(updated_result));
  }
}

void PrefsMigrator::DeleteOldPrefsEntryIfFullyMigrated(
    Config* config,
    std::optional<SelectedSegment> old_result) {
  // If the model has been migrated, delete the entry from the old prefs.
  if (!metadata_utils::ConfigUsesLegacyOutput(config) && old_result) {
    // Decide when to delete old result.
    old_prefs_->SaveSegmentationResultToPref(config->segmentation_key,
                                             std::nullopt);
  }
}

}  // namespace segmentation_platform
