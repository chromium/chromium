// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_STARTUP_MAINTENANCE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_STARTUP_MAINTENANCE_TASK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"

namespace offline_pages {

class ArchiveManager;
class OfflinePageMetadataStore;

// This task is responsible for executing maintenance sub-tasks during Chrome
// startup, including: temporary page consistency check, legacy directory
// cleaning and report storage usage UMA.
class StartupMaintenanceTask : public Task {
 public:
  StartupMaintenanceTask(OfflinePageMetadataStore* store,
                         ArchiveManager* archive_manager);

  StartupMaintenanceTask(const StartupMaintenanceTask&) = delete;
  StartupMaintenanceTask& operator=(const StartupMaintenanceTask&) = delete;

  ~StartupMaintenanceTask() override;

 private:
  // Task implementation:
  void Run() override;

  void OnStartupMaintenanceDone(bool result);

  // The store containing the offline pages. Not owned.
  raw_ptr<OfflinePageMetadataStore> store_;
  // The archive manager storing archive directories. Not owned.
  raw_ptr<ArchiveManager> archive_manager_;

  base::WeakPtrFactory<StartupMaintenanceTask> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_STARTUP_MAINTENANCE_TASK_H_
