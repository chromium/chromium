// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "components/desks_storage/core/desk_model.h"

namespace ash {
class DeskTemplate;
}

namespace desks_storage {

// The LocalDeskDataManager is the local storage implementation of
// the DeskModel interface and handles storage operations for local
// desk templates.
//
// TODO(crbug: 1227215): add calls to DeskModelObserver
class LocalDeskDataManager : public DeskModel {
 public:
  explicit LocalDeskDataManager(const base::FilePath& path);

  LocalDeskDataManager(const LocalDeskDataManager&) = delete;
  LocalDeskDataManager& operator=(const LocalDeskDataManager&) = delete;
  ~LocalDeskDataManager() override;

  // DeskModel:
  void GetAllEntries(GetAllEntriesCallback callback) override;
  void DeleteAllEntries(DeleteEntryCallback callback) override;
  void GetEntryByUUID(const std::string& uuid,
                      GetEntryByUuidCallback callback) override;
  void AddOrUpdateEntry(std::unique_ptr<ash::DeskTemplate> new_entry,
                        AddOrUpdateEntryCallback callback) override;
  void DeleteEntry(const std::string& uuid,
                   DeleteEntryCallback callback) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // This file path points to the user data directory's subdirectory:
  // "/path/to/user/data/dir/templates"
  const base::FilePath local_path_;
  // This vector is used so that this class will own desk_templates
  // retrieved via GetAllEntries.
  //
  // TODO implement full cache model instead of using this for just
  // GetAllEntries.
  std::vector<std::unique_ptr<ash::DeskTemplate>> desk_template_entries_;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_H_
