// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_DEFAULT_CORPUS_CHANGE_LIST_LOADER_H_
#define COMPONENTS_DRIVE_CHROMEOS_DEFAULT_CORPUS_CHANGE_LIST_LOADER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"
#include "components/drive/chromeos/about_resource_root_folder_id_loader.h"
#include "components/drive/chromeos/change_list_loader.h"
#include "components/drive/chromeos/directory_loader.h"
#include "components/drive/chromeos/drive_change_list_loader.h"
#include "components/drive/chromeos/start_page_token_loader.h"
#include "components/drive/chromeos/team_drive_list_loader.h"

namespace drive {

class EventLogger;
class JobScheduler;

namespace internal {

class AboutResourceLoader;
class ChangeListLoader;
class DirectoryLoader;
class LoaderController;
class ResourceMetadata;

// Loads change lists, the full resource list, and directory contents for the
// users default corpus.
class DefaultCorpusChangeListLoader : public DriveChangeListLoader {
 public:
  // |clock| can be mocked for testing.
  DefaultCorpusChangeListLoader(
      EventLogger* logger,
      base::SequencedTaskRunner* blocking_task_runner,
      ResourceMetadata* resource_metadata,
      JobScheduler* scheduler,
      AboutResourceLoader* about_resource_loader,
      LoaderController* apply_task_controller,
      const base::Clock* clock = base::DefaultClock::GetInstance());

  ~DefaultCorpusChangeListLoader() override;

  // DriveChangeListLoader overrides
  void AddChangeListLoaderObserver(ChangeListLoaderObserver* observer) override;
  void RemoveChangeListLoaderObserver(
      ChangeListLoaderObserver* observer) override;
  void AddTeamDriveListObserver(TeamDriveListObserver* observer) override;
  void RemoveTeamDriveListObserver(TeamDriveListObserver* observer) override;

  bool IsRefreshing() override;
  void LoadIfNeeded(const FileOperationCallback& callback) override;
  void ReadDirectory(const base::FilePath& directory_path,
                     ReadDirectoryEntriesCallback entries_callback,
                     const FileOperationCallback& completion_callback) override;
  void CheckForUpdates(const FileOperationCallback& callback) override;

 private:
  // Called after calling LoadIfNeeded on team drives.
  void OnChangeListLoadIfNeeded(const FileOperationCallback& callback,
                                FileError error);

  std::unique_ptr<internal::AboutResourceRootFolderIdLoader>
      root_folder_id_loader_;
  std::unique_ptr<internal::ChangeListLoader> change_list_loader_;
  std::unique_ptr<internal::DirectoryLoader> directory_loader_;
  std::unique_ptr<internal::StartPageTokenLoader> start_page_token_loader_;
  std::unique_ptr<TeamDriveListLoader> team_drive_list_loader_;

  EventLogger* logger_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ResourceMetadata* resource_metadata_;         // Not owned.
  JobScheduler* scheduler_;                     // Not owned.
  LoaderController* loader_controller_;         // Not owned.

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<DefaultCorpusChangeListLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DefaultCorpusChangeListLoader);
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_DEFAULT_CORPUS_CHANGE_LIST_LOADER_H_
