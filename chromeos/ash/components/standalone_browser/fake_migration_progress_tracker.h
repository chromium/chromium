// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_FAKE_MIGRATION_PROGRESS_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_FAKE_MIGRATION_PROGRESS_TRACKER_H_

#include "chromeos/ash/components/standalone_browser/migration_progress_tracker.h"

namespace ash::standalone_browser {

class FakeMigrationProgressTracker : public MigrationProgressTracker {
 public:
  FakeMigrationProgressTracker() = default;
  ~FakeMigrationProgressTracker() override = default;
  FakeMigrationProgressTracker(const FakeMigrationProgressTracker&) = delete;
  FakeMigrationProgressTracker& operator=(const FakeMigrationProgressTracker&) =
      delete;

  void UpdateProgress(int64_t size) override {}
  void SetTotalSizeToCopy(int64_t size) override {}
};

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_FAKE_MIGRATION_PROGRESS_TRACKER_H_
