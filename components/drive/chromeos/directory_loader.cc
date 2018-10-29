// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/directory_loader.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/drive/chromeos/change_list_loader_observer.h"
#include "components/drive/chromeos/change_list_processor.h"
#include "components/drive/chromeos/drive_file_util.h"
#include "components/drive/chromeos/loader_controller.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/chromeos/root_folder_id_loader.h"
#include "components/drive/chromeos/start_page_token_loader.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"
#include "google_apis/drive/drive_api_parser.h"
#include "url/gurl.h"

namespace drive {
namespace internal {

namespace {

// Minimum changestamp gap required to start loading directory.
constexpr int kMinimumChangestampGap = 50;

constexpr base::TimeDelta kMinimumPerDirectoryFetchTimeGap =
    base::TimeDelta::FromSeconds(30);

FileError CheckLocalState(ResourceMetadata* resource_metadata,
                          const base::FilePath& root_entry_path,
                          const std::string& team_drive_id,
                          const std::string& root_folder_id,
                          const std::string& local_id,
                          ResourceEntry* entry,
                          std::string* start_page_token) {
  DCHECK(start_page_token);
  // Fill My Drive resource ID.
  ResourceEntry mydrive;
  FileError error =
      resource_metadata->GetResourceEntryByPath(root_entry_path, &mydrive);
  if (error != FILE_ERROR_OK)
    return error;

  if (mydrive.resource_id().empty()) {
    mydrive.set_resource_id(root_folder_id);
    error = resource_metadata->RefreshEntry(mydrive);
    if (error != FILE_ERROR_OK)
      return error;
  }

  // Get entry.
  error = resource_metadata->GetResourceEntryById(local_id, entry);
  if (error != FILE_ERROR_OK)
    return error;

  // Get the local start page token..
  return GetStartPageToken(resource_metadata, team_drive_id, start_page_token);
}

FileError UpdateStartPageToken(ResourceMetadata* resource_metadata,
                               const DirectoryFetchInfo& directory_fetch_info,
                               base::FilePath* directory_path,
                               const base::Clock* clock) {
  DCHECK(clock);
  // Update the directory start page token.
  ResourceEntry directory;
  FileError error = resource_metadata->GetResourceEntryById(
      directory_fetch_info.local_id(), &directory);
  if (error != FILE_ERROR_OK)
    return error;

  if (!directory.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  DirectorySpecificInfo* directory_specific_info =
      directory.mutable_directory_specific_info();

  directory_specific_info->set_start_page_token(
      directory_fetch_info.start_page_token());
  directory_specific_info->set_last_read_time_ms(
      clock->Now().ToDeltaSinceWindowsEpoch().InMilliseconds());

  error = resource_metadata->RefreshEntry(directory);
  if (error != FILE_ERROR_OK)
    return error;

  // Get the directory path.
  return resource_metadata->GetFilePath(directory_fetch_info.local_id(),
                                        directory_path);
}

}  // namespace

struct DirectoryLoader::ReadDirectoryCallbackState {
  explicit ReadDirectoryCallbackState(
      ReadDirectoryEntriesCallback entries_callback)
      : entries_callback(std::move(entries_callback)) {}

  ReadDirectoryEntriesCallback entries_callback;
  FileOperationCallback completion_callback;
  std::set<std::string> sent_entry_names;
};

// Fetches the resource entries in the directory with |directory_resource_id|.
class DirectoryLoader::FeedFetcher {
 public:
  FeedFetcher(DirectoryLoader* loader,
              const DirectoryFetchInfo& directory_fetch_info,
              const std::string& root_folder_id)
      : loader_(loader),
        directory_fetch_info_(directory_fetch_info),
        root_folder_id_(root_folder_id),
        weak_ptr_factory_(this) {
  }

  ~FeedFetcher() = default;

  void Run(const FileOperationCallback& callback) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback);
    DCHECK(!directory_fetch_info_.resource_id().empty());

    // Remember the time stamp for usage stats.
    start_time_ = base::TimeTicks::Now();

    loader_->scheduler_->GetFileListInDirectory(
        directory_fetch_info_.resource_id(),
        base::Bind(&FeedFetcher::OnFileListFetched,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

 private:
  void OnFileListFetched(const FileOperationCallback& callback,
                         google_apis::DriveApiErrorCode status,
                         std::unique_ptr<google_apis::FileList> file_list) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback);

    FileError error = GDataToFileError(status);
    if (error != FILE_ERROR_OK) {
      callback.Run(error);
      return;
    }

    DCHECK(file_list);
    std::unique_ptr<ChangeList> change_list(new ChangeList(*file_list));
    GURL next_url = file_list->next_link();

    ResourceEntryVector* entries = new ResourceEntryVector;
    loader_->loader_controller_->ScheduleRun(base::BindOnce(
        &drive::util::RunAsyncTask,
        base::RetainedRef(loader_->blocking_task_runner_), FROM_HERE,
        base::BindOnce(&ChangeListProcessor::RefreshDirectory,
                       loader_->resource_metadata_, directory_fetch_info_,
                       std::move(change_list), entries),
        base::BindOnce(&FeedFetcher::OnDirectoryRefreshed,
                       weak_ptr_factory_.GetWeakPtr(), callback, next_url,
                       base::Owned(entries))));
  }

  void OnDirectoryRefreshed(
      const FileOperationCallback& callback,
      const GURL& next_url,
      const std::vector<ResourceEntry>* refreshed_entries,
      FileError error) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(callback);

    if (error != FILE_ERROR_OK) {
      callback.Run(error);
      return;
    }

    loader_->SendEntries(directory_fetch_info_.local_id(), *refreshed_entries);

    if (!next_url.is_empty()) {
      // There is the remaining result so fetch it.
      loader_->scheduler_->GetRemainingFileList(
          next_url,
          base::Bind(&FeedFetcher::OnFileListFetched,
                     weak_ptr_factory_.GetWeakPtr(), callback));
      return;
    }

    base::TimeDelta duration = base::TimeTicks::Now() - start_time_;
    if (util::IsTeamDrivesPath(directory_fetch_info_.root_entry_path())) {
      UMA_HISTOGRAM_TIMES("Drive.DirectoryFeedLoadTime.TeamDrives", duration);
    } else {
      UMA_HISTOGRAM_TIMES("Drive.DirectoryFeedLoadTime", duration);
    }

    // Note: The fetcher is managed by DirectoryLoader, and the instance
    // will be deleted in the callback. Do not touch the fields after this
    // invocation.
    callback.Run(FILE_ERROR_OK);
  }

  DirectoryLoader* loader_;
  DirectoryFetchInfo directory_fetch_info_;
  std::string root_folder_id_;
  base::TimeTicks start_time_;
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<FeedFetcher> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FeedFetcher);
};

DirectoryLoader::DirectoryLoader(
    EventLogger* logger,
    base::SequencedTaskRunner* blocking_task_runner,
    ResourceMetadata* resource_metadata,
    JobScheduler* scheduler,
    RootFolderIdLoader* root_folder_id_loader,
    StartPageTokenLoader* start_page_token_loader,
    LoaderController* loader_controller,
    const base::FilePath& root_entry_path,
    const std::string& team_drive_id,
    const base::Clock* clock)
    : logger_(logger),
      blocking_task_runner_(blocking_task_runner),
      resource_metadata_(resource_metadata),
      scheduler_(scheduler),
      root_folder_id_loader_(root_folder_id_loader),
      start_page_token_loader_(start_page_token_loader),
      loader_controller_(loader_controller),
      root_entry_path_(root_entry_path),
      team_drive_id_(team_drive_id),
      clock_(clock),
      weak_ptr_factory_(this) {}

DirectoryLoader::~DirectoryLoader() = default;

void DirectoryLoader::AddObserver(ChangeListLoaderObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);
}

void DirectoryLoader::RemoveObserver(ChangeListLoaderObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

void DirectoryLoader::ReadDirectory(
    const base::FilePath& directory_path,
    ReadDirectoryEntriesCallback entries_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(completion_callback);

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(resource_metadata_), directory_path, entry),
      base::Bind(&DirectoryLoader::ReadDirectoryAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(), directory_path,
                 base::Passed(std::move(entries_callback)), completion_callback,
                 true,  // should_try_loading_parent
                 base::Owned(entry)));
}

void DirectoryLoader::ReadDirectoryAfterGetEntry(
    const base::FilePath& directory_path,
    ReadDirectoryEntriesCallback entries_callback,
    const FileOperationCallback& completion_callback,
    bool should_try_loading_parent,
    const ResourceEntry* entry,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(completion_callback);

  if (error == FILE_ERROR_NOT_FOUND &&
      should_try_loading_parent &&
      util::GetDriveGrandRootPath().IsParent(directory_path)) {
    // This entry may be found after loading the parent.
    ReadDirectory(directory_path.DirName(), ReadDirectoryEntriesCallback(),
                  base::Bind(&DirectoryLoader::ReadDirectoryAfterLoadParent,
                             weak_ptr_factory_.GetWeakPtr(), directory_path,
                             base::Passed(std::move(entries_callback)),
                             completion_callback));
    return;
  }
  if (error != FILE_ERROR_OK) {
    completion_callback.Run(error);
    return;
  }

  if (!entry->file_info().is_directory()) {
    completion_callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  DirectoryFetchInfo directory_fetch_info(
      entry->local_id(), entry->resource_id(),
      entry->directory_specific_info().start_page_token(), root_entry_path_,
      directory_path);

  // Register the callback function to be called when it is loaded.
  const std::string& local_id = directory_fetch_info.local_id();
  ReadDirectoryCallbackState callback_state(std::move(entries_callback));
  callback_state.completion_callback = completion_callback;
  pending_load_callback_[local_id].emplace_back(std::move(callback_state));

  // If loading task for |local_id| is already running, do nothing.
  if (pending_load_callback_[local_id].size() > 1)
    return;

  root_folder_id_loader_->GetRootFolderId(
      base::Bind(&DirectoryLoader::ReadDirectoryAfterGetRootFolderId,
                 weak_ptr_factory_.GetWeakPtr(), directory_path, local_id));
}

void DirectoryLoader::ReadDirectoryAfterLoadParent(
    const base::FilePath& directory_path,
    ReadDirectoryEntriesCallback entries_callback,
    const FileOperationCallback& completion_callback,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(completion_callback);

  if (error != FILE_ERROR_OK) {
    completion_callback.Run(error);
    return;
  }

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(resource_metadata_), directory_path, entry),
      base::Bind(&DirectoryLoader::ReadDirectoryAfterGetEntry,
                 weak_ptr_factory_.GetWeakPtr(), directory_path,
                 base::Passed(std::move(entries_callback)), completion_callback,
                 false,  // should_try_loading_parent
                 base::Owned(entry)));
}

void DirectoryLoader::ReadDirectoryAfterGetRootFolderId(
    const base::FilePath& directory_path,
    const std::string& local_id,
    FileError error,
    base::Optional<std::string> root_folder_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (error != FILE_ERROR_OK) {
    OnDirectoryLoadComplete(local_id, error);
    return;
  }

  DCHECK(root_folder_id);

  start_page_token_loader_->GetStartPageToken(
      base::Bind(&DirectoryLoader::ReadDirectoryAfterGetStartPageToken,
                 weak_ptr_factory_.GetWeakPtr(), directory_path, local_id,
                 root_folder_id.value()));
}

void DirectoryLoader::ReadDirectoryAfterGetStartPageToken(
    const base::FilePath& directory_path,
    const std::string& local_id,
    const std::string& root_folder_id,
    google_apis::DriveApiErrorCode status,
    std::unique_ptr<google_apis::StartPageToken> start_page_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    OnDirectoryLoadComplete(local_id, error);
    return;
  }

  DCHECK(start_page_token);

  // Check the current status of local metadata, and start loading if needed.
  ResourceEntry* entry = new ResourceEntry;
  std::string* local_start_page_token = new std::string();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&CheckLocalState, resource_metadata_, root_entry_path_,
                     team_drive_id_, root_folder_id, local_id, entry,
                     local_start_page_token),
      base::BindOnce(&DirectoryLoader::ReadDirectoryAfterCheckLocalState,
                     weak_ptr_factory_.GetWeakPtr(), directory_path,
                     start_page_token->start_page_token(), local_id,
                     root_folder_id, base::Owned(entry),
                     base::Owned(local_start_page_token)));
}

void DirectoryLoader::ReadDirectoryAfterCheckLocalState(
    const base::FilePath& directory_path,
    const std::string& remote_start_page_token,
    const std::string& local_id,
    const std::string& root_folder_id,
    const ResourceEntry* entry,
    const std::string* local_start_page_token,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(local_start_page_token);

  if (error != FILE_ERROR_OK) {
    OnDirectoryLoadComplete(local_id, error);
    return;
  }
  // This entry does not exist on the server.
  if (entry->resource_id().empty()) {
    OnDirectoryLoadComplete(local_id, FILE_ERROR_OK);
    return;
  }

  // Start loading the directory.
  const std::string& directory_start_page_token =
      entry->directory_specific_info().start_page_token();

  DirectoryFetchInfo directory_fetch_info(local_id, entry->resource_id(),
                                          remote_start_page_token,
                                          root_entry_path_, directory_path);

  int64_t directory_changestamp = 0;
  // The directory_specific_info may be enpty, so default changestamp to 0.
  if (!directory_start_page_token.empty() &&
      !drive::util::ConvertStartPageTokenToChangestamp(
          directory_start_page_token, &directory_changestamp)) {
    logger_->Log(
        logging::LOG_ERROR,
        "Unable to convert directory start page tokens to changestamps, will "
        "load directory from server %s; directory start page token: %s ",
        directory_fetch_info.ToString().c_str(),
        directory_start_page_token.c_str());
    LoadDirectoryFromServer(directory_fetch_info, root_folder_id);
    return;
  }

  // If we haven't finished loading the drive corpus, local_start_page_token
  // will be empty. In this case we will keep track of the last time we loaded
  // the directory, and only reload after an appropriate delay.
  if (local_start_page_token->empty()) {
    if (entry->directory_specific_info().has_last_read_time_ms()) {
      base::Time last_read = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMilliseconds(
              entry->directory_specific_info().last_read_time_ms()));
      base::TimeDelta elapsed = clock_->Now() - last_read;
      if (elapsed < kMinimumPerDirectoryFetchTimeGap) {
        logger_->Log(
            logging::LOG_INFO,
            "Skipping read of directory (%s), contents considered fresh.",
            directory_fetch_info.ToString().c_str());
        OnDirectoryLoadComplete(local_id, FILE_ERROR_OK);
        return;
      }
    }
  }

  int64_t remote_changestamp = 0;
  int64_t local_changestamp = 0;
  if (!drive::util::ConvertStartPageTokenToChangestamp(remote_start_page_token,
                                                       &remote_changestamp) ||
      !drive::util::ConvertStartPageTokenToChangestamp(*local_start_page_token,
                                                       &local_changestamp)) {
    logger_->Log(
        logging::LOG_ERROR,
        "Unable to convert start page tokens to changestamps, will load "
        "directory from server %s; local start page token: %s; "
        "remote start page token: %s",
        directory_fetch_info.ToString().c_str(),
        local_start_page_token->c_str(), remote_start_page_token.c_str());
    LoadDirectoryFromServer(directory_fetch_info, root_folder_id);
    return;
  }

  // Start loading the directory.
  directory_changestamp = std::max(directory_changestamp, local_changestamp);

  // If the directory's changestamp is up to date or the global changestamp or
  // the metadata DB is new enough (which means the normal changelist loading
  // should finish very soon), just schedule to run the callback, as there is no
  // need to fetch the directory.
  if (directory_changestamp >= remote_changestamp ||
      local_changestamp + kMinimumChangestampGap > remote_changestamp) {
    OnDirectoryLoadComplete(local_id, FILE_ERROR_OK);
  } else {
    // Start fetching the directory content, and mark it with the changestamp
    // |remote_changestamp|.
    LoadDirectoryFromServer(directory_fetch_info, root_folder_id);
  }
}

void DirectoryLoader::OnDirectoryLoadComplete(const std::string& local_id,
                                              FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LoadCallbackMap::iterator it = pending_load_callback_.find(local_id);
  if (it == pending_load_callback_.end())
    return;

  // No need to read metadata when no one needs entries.
  bool needs_to_send_entries = false;
  for (size_t i = 0; i < it->second.size(); ++i) {
    const ReadDirectoryCallbackState& callback_state = it->second[i];
    if (callback_state.entries_callback)
      needs_to_send_entries = true;
  }

  if (!needs_to_send_entries) {
    OnDirectoryLoadCompleteAfterRead(local_id, nullptr, FILE_ERROR_OK);
    return;
  }

  ResourceEntryVector* entries = new ResourceEntryVector;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::ReadDirectoryById,
                 base::Unretained(resource_metadata_), local_id, entries),
      base::Bind(&DirectoryLoader::OnDirectoryLoadCompleteAfterRead,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_id,
                 base::Owned(entries)));
}

void DirectoryLoader::OnDirectoryLoadCompleteAfterRead(
    const std::string& local_id,
    const ResourceEntryVector* entries,
    FileError error) {
  LoadCallbackMap::iterator it = pending_load_callback_.find(local_id);
  if (it != pending_load_callback_.end()) {
    DVLOG(1) << "Running callback for " << local_id;

    if (error == FILE_ERROR_OK && entries)
      SendEntries(local_id, *entries);

    for (size_t i = 0; i < it->second.size(); ++i) {
      const ReadDirectoryCallbackState& callback_state = it->second[i];
      callback_state.completion_callback.Run(error);
    }
    pending_load_callback_.erase(it);
  }
}

void DirectoryLoader::SendEntries(const std::string& local_id,
                                  const ResourceEntryVector& entries) {
  LoadCallbackMap::iterator it = pending_load_callback_.find(local_id);
  DCHECK(it != pending_load_callback_.end()) << "local_id: " << local_id;

  for (size_t i = 0; i < it->second.size(); ++i) {
    ReadDirectoryCallbackState* callback_state = &it->second[i];
    if (!callback_state->entries_callback)
      continue;

    // Filter out entries which were already sent.
    std::unique_ptr<ResourceEntryVector> entries_to_send(
        new ResourceEntryVector);
    for (size_t i = 0; i < entries.size(); ++i) {
      const ResourceEntry& entry = entries[i];
      if (!callback_state->sent_entry_names.count(entry.base_name())) {
        callback_state->sent_entry_names.insert(entry.base_name());
        entries_to_send->push_back(entry);
      }
    }
    std::move(callback_state->entries_callback).Run(std::move(entries_to_send));
  }
}

void DirectoryLoader::LoadDirectoryFromServer(
    const DirectoryFetchInfo& directory_fetch_info,
    const std::string& root_folder_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!directory_fetch_info.empty());
  DVLOG(1) << "Start loading directory: " << directory_fetch_info.ToString();

  const google_apis::StartPageToken* start_page_token =
      start_page_token_loader_->cached_start_page_token();
  DCHECK(start_page_token);

  logger_->Log(logging::LOG_INFO,
               "Fast-fetch start: %s; Server start page token: %s",
               directory_fetch_info.ToString().c_str(),
               start_page_token->start_page_token().c_str());

  FeedFetcher* fetcher =
      new FeedFetcher(this, directory_fetch_info, root_folder_id);

  fast_fetch_feed_fetcher_set_.insert(base::WrapUnique(fetcher));
  fetcher->Run(
      base::Bind(&DirectoryLoader::LoadDirectoryFromServerAfterLoad,
                 weak_ptr_factory_.GetWeakPtr(),
                 directory_fetch_info,
                 fetcher));
}

void DirectoryLoader::LoadDirectoryFromServerAfterLoad(
    const DirectoryFetchInfo& directory_fetch_info,
    FeedFetcher* fetcher,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!directory_fetch_info.empty());

  // Delete the fetcher.
  auto it = std::find_if(fast_fetch_feed_fetcher_set_.begin(),
                         fast_fetch_feed_fetcher_set_.end(),
                         [fetcher](const std::unique_ptr<FeedFetcher>& ptr) {
                           return ptr.get() == fetcher;
                         });
  fast_fetch_feed_fetcher_set_.erase(it);

  logger_->Log(logging::LOG_INFO,
               "Fast-fetch complete: %s => %s",
               directory_fetch_info.ToString().c_str(),
               FileErrorToString(error).c_str());

  if (error != FILE_ERROR_OK) {
    LOG(ERROR) << "Failed to load directory: "
               << directory_fetch_info.local_id()
               << ": " << FileErrorToString(error);
    OnDirectoryLoadComplete(directory_fetch_info.local_id(), error);
    return;
  }

  // Update start page token and get the directory path.
  base::FilePath* directory_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&UpdateStartPageToken, resource_metadata_,
                 directory_fetch_info, directory_path,
                 base::Unretained(clock_)),
      base::Bind(
          &DirectoryLoader::LoadDirectoryFromServerAfterUpdateStartPageToken,
          weak_ptr_factory_.GetWeakPtr(), directory_fetch_info,
          base::Owned(directory_path)));
}

void DirectoryLoader::LoadDirectoryFromServerAfterUpdateStartPageToken(
    const DirectoryFetchInfo& directory_fetch_info,
    const base::FilePath* directory_path,
    FileError error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DVLOG(1) << "Directory loaded: " << directory_fetch_info.ToString();
  OnDirectoryLoadComplete(directory_fetch_info.local_id(), error);

  // Also notify the observers.
  if (error == FILE_ERROR_OK && !directory_path->empty()) {
    for (auto& observer : observers_)
      observer.OnDirectoryReloaded(*directory_path);
  }
}

}  // namespace internal
}  // namespace drive
