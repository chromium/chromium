// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_MIGRATION_PROGRESS_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_MIGRATION_PROGRESS_TRACKER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace ash::standalone_browser {

using ProgressCallback = base::RepeatingCallback<void(int)>;

// Interface to be inherited by `MigrationProgressTrackerImpl` and
// `FakeMigrationProgressTrackerImpl`. It is passed to
// `BrowserDataMigrator::MigrateInternal()` and called whenever there is an
// update in progress of the migration.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
    MigrationProgressTracker {
 public:
  virtual ~MigrationProgressTracker() = default;
  virtual void SetTotalSizeToCopy(int64_t size) = 0;
  virtual void UpdateProgress(int64_t size) = 0;
};

// Used to send progress updates to the UI. `progress_callback_` is posted on
// the UI thread.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
  MigrationProgressTrackerImpl : public MigrationProgressTracker {
 public:
  explicit MigrationProgressTrackerImpl(const ProgressCallback& callback);
  MigrationProgressTrackerImpl(const MigrationProgressTrackerImpl&) = delete;
  MigrationProgressTrackerImpl& operator=(const MigrationProgressTrackerImpl&) =
      delete;
  ~MigrationProgressTrackerImpl() override;

  // Updates `size_copied_`. If `progress_` gets updated subsequently, meaning
  // there is a more than 1% change in progress, then it calls
  // `progress_callback_` with the new progress value.
  void UpdateProgress(int64_t size) override;

  void SetTotalSizeToCopy(int64_t size) override;

 private:
  // % of migration that is done. Equivalent to size_copied_ * 100 /
  // total_size_to_copy_.
  int progress_ = 0;
  // Data copied so far in bytes.
  int64_t size_copied_ = 0;
  // The total size of data that has to be copied in bytes.
  int64_t total_size_to_copy_ = -1;
  // A callback passe
  ProgressCallback progress_callback_;
};

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_MIGRATION_PROGRESS_TRACKER_H_
