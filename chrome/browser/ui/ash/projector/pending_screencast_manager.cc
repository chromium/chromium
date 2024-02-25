// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/pending_screencast_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/projector/projector_metrics.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom-forward.h"
#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"

namespace {

constexpr char kOpenUrlBase[] = "https://drive.google.com/open";
constexpr char kDriveRequestContentHintsKey[] = "contentHints";
constexpr char kDriveRequestIndexableTextKey[] = "indexableText";

// The metadata might not be ready as the file gets uploaded. On projector app
// side, we fetch newly uploaded screencasts with 2s delay, and it works fine,
// so put 3s here to allow Drive to populate the metadata.
constexpr base::TimeDelta kDriveGetMetadataDelay = base::Seconds(3);

bool IsWebmOrProjectorFile(const base::FilePath& path) {
  return IsMediaFile(path) || IsMetadataFile(path);
}

// "Absolute path" is the DriveFS absolute path of `drive_relative_path` on
// local file system, for example: absolute_path =
// "/{$drivefs_mounted_point}/root/{$drive_relative_path}";
base::FilePath GetLocalAbsolutePath(const base::FilePath& drivefs_mounted_point,
                                    const base::FilePath& drive_relative_path) {
  base::FilePath root("/");
  base::FilePath absolute_path(drivefs_mounted_point);
  root.AppendRelativePath(drive_relative_path, &absolute_path);
  return absolute_path;
}

// Returns the Drive server side id from |url| e.g.
// https://drive.google.com/open?id=[ID].
std::optional<std::string> GetIdFromDriveUrl(const GURL& url) {
  const std::string& spec = url.spec();
  if (!base::StartsWith(spec, kOpenUrlBase,
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return std::nullopt;
  }
  std::string id;
  if (!net::GetValueForKeyInQuery(url, "id", &id)) {
    return std::nullopt;
  }
  return id;
}

// Retrieves the file id from `metadata` and runs the `get_file_id_callback`
// callback.
void ParseFileIdOnGetMetaData(
    PendingScreencastManager::OnGetFileIdCallback get_file_id_callback,
    const base::FilePath& local_file_path,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  std::string file_id;
  // TODO(b/232282526): Add metric to track how often we get metadata failed.
  if (error != drive::FileError::FILE_ERROR_OK || !metadata) {
    LOG(ERROR) << "Get Drive File metadata failed";
  } else if (metadata->alternate_url.empty()) {
    LOG(ERROR) << "No alternate_url found in file metadata";
  } else {
    // TODO(b/221078840): Use the file id directly when it is available in
    // `metadata`.
    std::optional<std::string> parsed_file_id =
        GetIdFromDriveUrl(GURL(metadata->alternate_url));
    if (parsed_file_id.has_value()) {
      file_id = parsed_file_id.value();
    } else {
      LOG(ERROR) << "Could not get file id from alternate url";
    }
  }

  std::move(get_file_id_callback).Run(local_file_path, file_id);
}

// Gets the absolute path for `drive_relative_path` and gets Drive metadata for
// the given file path. To execute the `callback`, we need to know the server
// side file id, which could be learned from metadata.
void GetDriveFileMetadata(
    const base::FilePath& drive_relative_path,
    PendingScreencastManager::OnGetFileIdCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  auto* drive_integration_service =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  if (!drive_integration_service) {
    return;
  }
  const base::FilePath local_path = GetLocalAbsolutePath(
      drive_integration_service->GetMountPointPath(), drive_relative_path);

  // drive::DriveIntegrationService::GetMetadata should only be called on UI
  // thread.
  drive_integration_service->GetMetadata(
      local_path, base::BindOnce(&ParseFileIdOnGetMetaData, std::move(callback),
                                 local_path));
}

// Reads the screencast metadata file from `metadata_file_local_path`. A sample
// file content:
// {
//   "captionLanguage":"en",
//   "captions":[
//     {
//      "endOffset":1260,
//      "hypothesisParts:[],
//      "startOffset":760,
//      "text":"abcd",
//     }
//   ],
//   "tableOfContent":[]
// }
// Returns the indexable text concated by all "text" fields content.
std::string GetIndexableText(const base::FilePath& metadata_file_local_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string indexable_text = "";

  // Reads the Json content in `metadata_file_local_path` to `dict_value`:
  std::string file_content;
  if (!base::ReadFileToString(metadata_file_local_path, &file_content)) {
    return indexable_text;
  }

  std::optional<base::Value> value(base::JSONReader::Read(file_content));
  if (!value) {
    return indexable_text;
  }

  const base::Value::Dict* dict_value = value.value().GetIfDict();
  if (!dict_value) {
    return indexable_text;
  }

  // Concats all captions' text:
  const auto* captions = dict_value->FindList("captions");
  if (!captions) {
    return indexable_text;
  }

  for (const auto& caption : *captions) {
    const base::Value::Dict* caption_dict = caption.GetIfDict();
    if (!caption_dict) {
      continue;
    }
    const std::string* text = caption_dict->FindString("text");
    if (text && !text->empty()) {
      base::StrAppend(&indexable_text, {" ", *text});
    }
  }
  return indexable_text;
}

// Returns the request body, which looks like:
// {
//   "contentHints":
//     {
//      "indexableText":"abcd",
//     }
// }
const std::string BuildRequestBody(
    const base::FilePath& metadata_file_local_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const std::string indexable_text = GetIndexableText(metadata_file_local_path);
  if (indexable_text.empty()) {
    return std::string();
  }

  // Builds request body:
  base::Value::Dict root;
  base::Value::Dict contentHints;
  contentHints.Set(kDriveRequestIndexableTextKey, indexable_text);
  root.Set(kDriveRequestContentHintsKey, std::move(contentHints));

  std::string request_body;
  base::JSONWriter::Write(std::move(root), &request_body);

  return request_body;
}

// Returns a valid pending screencast from `container_absolute_path`.  A valid
// screencast should have 1 media file and 1 metadata file.
std::optional<ash::PendingScreencastContainer> GetPendingScreencastContainer(
    const base::FilePath& container_dir,
    const base::FilePath& drivefs_mounted_point,
    bool upload_failed) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const base::FilePath container_absolute_path =
      GetLocalAbsolutePath(drivefs_mounted_point, container_dir);
  if (!base::PathExists(container_absolute_path)) {
    return std::nullopt;
  }

  int64_t total_size_in_bytes = 0;
  int media_file_count = 0;
  int metadata_file_count = 0;

  base::Time created_time;
  std::string media_name;

  base::FileEnumerator files(container_absolute_path, /*recursive=*/false,
                             base::FileEnumerator::FILES);

  // Calculates the size of media file and metadata file, and the created time
  // of media.
  for (base::FilePath path = files.Next(); !path.empty(); path = files.Next()) {
    if (IsMetadataFile(path)) {
      total_size_in_bytes += files.GetInfo().GetSize();
      metadata_file_count++;
    } else if (IsMediaFile(path)) {
      base::File::Info info;
      if (!base::GetFileInfo(path, &info)) {
        continue;
      }
      created_time = info.creation_time;
      total_size_in_bytes += files.GetInfo().GetSize();
      media_name = path.BaseName().RemoveExtension().value();
      media_file_count++;
    }

    // Return null if the screencast is not valid.
    if (media_file_count > 1 || metadata_file_count > 1) {
      return std::nullopt;
    }
  }

  // Return null if the screencast is not valid.
  if (media_file_count != 1 || metadata_file_count != 1) {
    return std::nullopt;
  }

  ash::PendingScreencastContainer pending_screencast{container_dir};
  pending_screencast.SetTotalSizeInBytes(total_size_in_bytes);
  pending_screencast.SetName(media_name);
  pending_screencast.SetCreatedTime(created_time);
  pending_screencast.set_upload_failed(upload_failed);

  return pending_screencast;
}

// The `pending_webm_or_projector_events` are new uploading ".webm" or
// ".projector" files' events. The `error_syncing_file` are ".webm" or
// ".projector" files which failed to upload. Checks whether these files are
// valid screencast files. Calculates the upload progress or error state and
// returns valid pending or error screencasts.
ash::PendingScreencastContainerSet ProcessAndGenerateNewScreencasts(
    const std::vector<drivefs::mojom::ItemEvent>&
        pending_webm_or_projector_events,
    const std::set<base::FilePath>& error_syncing_file,
    const base::FilePath drivefs_mounted_point) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // The valid screencasts set.
  ash::PendingScreencastContainerSet screencasts;

  if (!base::PathExists(drivefs_mounted_point) ||
      (pending_webm_or_projector_events.empty() &&
       error_syncing_file.empty())) {
    return screencasts;
  }

  // A map of container directory path to pending screencast. Each screencast
  // has a unique container directory path in DriveFS.
  std::map<base::FilePath, ash::PendingScreencastContainer>
      container_to_screencasts;

  // Creates error screencasts from `error_syncing_file`:
  for (const auto& upload_failed_file : error_syncing_file) {
    const base::FilePath container_dir = upload_failed_file.DirName();
    auto new_screencast = GetPendingScreencastContainer(
        container_dir, drivefs_mounted_point, /*upload_failed=*/true);
    if (new_screencast) {
      container_to_screencasts[container_dir] = new_screencast.value();
    }
  }

  // Creates uploading screencasts from `pending_webm_or_projector_events`:

  // The `pending_event.path` is the file path in drive. It looks like
  // "/root/{folder path in drive}/{file name}".
  for (const auto& pending_event : pending_webm_or_projector_events) {
    base::FilePath event_file = base::FilePath(pending_event.path);
    // `container_dir` is the parent folder of `pending_event.path` in drive. It
    // looks like "/root/{folder path in drive}".
    const base::FilePath container_dir = event_file.DirName();

    // During this loop, items of multiple events might be under the same
    // folder.
    auto iter = container_to_screencasts.find(container_dir);
    if (iter != container_to_screencasts.end()) {
      ash::PendingScreencastContainer& entry = iter->second;
      // Calculates remaining untranferred bytes of a screencast by adding up
      // its transferred bytes of its files. `pending_event.bytes_to_transfer`
      // is the total bytes of current file.
      // TODO(b/209854146) Not all files appear in
      // `pending_webm_or_projector_events.bytes_transferred`. The missing files
      // might be uploaded or not uploaded. To get an accurate
      // `bytes_transferred`, use DriveIntegrationService::GetMetadata().
      if (!entry.pending_screencast().upload_failed) {
        entry.SetTotalBytesTransferred(entry.bytes_transferred() +
                                       pending_event.bytes_transferred);
      }

      // Skips getting the size of a folder if it has been validated before.
      continue;
    }

    auto new_screencast = GetPendingScreencastContainer(
        container_dir, drivefs_mounted_point, /*upload_failed=*/false);

    if (new_screencast) {
      new_screencast->SetTotalBytesTransferred(pending_event.bytes_transferred);
      container_to_screencasts[container_dir] = new_screencast.value();
    }
  }

  for (const auto& pair : container_to_screencasts) {
    screencasts.insert(pair.second);
  }

  return screencasts;
}

}  // namespace

// Using base::Unretained for callback is safe since the
// PendingScreencastManager owns the `drive_helper_`.
PendingScreencastManager::PendingScreencastManager(
    PendingScreencastChangeCallback pending_screencast_change_callback)
    : pending_screencast_change_callback_(pending_screencast_change_callback),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      drive_helper_(base::BindRepeating(
          &PendingScreencastManager::MaybeSwitchDriveFsObservation,
          base::Unretained(this))) {}

PendingScreencastManager::~PendingScreencastManager() = default;

void PendingScreencastManager::OnUnmounted() {
  if (!pending_screencast_cache_.empty()) {
    pending_screencast_cache_.clear();
    // Since DriveFS is unmounted, screencasts stop uploading. Notifies pending
    // screencast status has changed.
    pending_screencast_change_callback_.Run(pending_screencast_cache_);
    last_pending_screencast_change_tick_ = base::TimeTicks();
  }
  error_syncing_files_.clear();
}

// Generates new pending upload screencasts list base on `error_syncing_files_`
// and files from drivefs::mojom::SyncingStatus.
//
// When file in error_syncing_files_ complete uploading, remove from
// `error_syncing_files_` so failed screencasts will be removed from pending
// screencast list.
// TODO(b/200343894): OnSyncingStatusUpdate() gets called for both upload and
// download event. Find a way to filter out the upload event.
void PendingScreencastManager::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& status) {
  if (!ProjectorDriveFsProvider::IsDriveFsMounted()) {
    return;
  }
  std::vector<drivefs::mojom::ItemEvent> pending_webm_or_projector_events;
  for (const auto& event : status.item_events) {
    const base::FilePath event_file = base::FilePath(event->path);

    if (event->state == drivefs::mojom::ItemEvent::State::kCompleted) {
      OnFileSyncedCompletely(event_file);
    }

    bool pending =
        event->state == drivefs::mojom::ItemEvent::State::kQueued ||
        event->state == drivefs::mojom::ItemEvent::State::kInProgress;
    // Filters pending ".webm" or ".projector".
    if (!pending || !IsWebmOrProjectorFile(event_file)) {
      continue;
    }

    // We might have received the same event with "kCompleted" state multiple
    // times. The `syncing_metadata_files_` is used to watch the first
    // "kCompleted" state for a file so that we could only update indexable text
    // once.
    if (ash::features::IsProjectorUpdateIndexableTextEnabled() &&
        IsMetadataFile(event_file)) {
      syncing_metadata_files_.emplace(event_file);
    }
    pending_webm_or_projector_events.emplace_back(*event.get());
  }

  // If the `pending_webm_or_projector_events`, `error_syncing_files_` and
  // `pending_screencast_cache_` are empty, return early because the syncing may
  // be triggered by files that are not related to Projector.
  if (pending_webm_or_projector_events.empty() &&
      error_syncing_files_.empty() && pending_screencast_cache_.empty()) {
    return;
  }

  // The `task` is a blocking I/O operation while `reply` runs on current
  // thread.
  // TODO(b/223668878) OnSyncingStatusUpdate might get called multiple times
  // within 1s. Add a repeat timer to trigger this task for less frequency.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ProcessAndGenerateNewScreencasts,
                     std::move(pending_webm_or_projector_events),
                     error_syncing_files_,
                     ProjectorDriveFsProvider::GetDriveFsMountPointPath()),
      base::BindOnce(
          &PendingScreencastManager::OnProcessAndGenerateNewScreencastsFinished,
          weak_ptr_factory_.GetWeakPtr(),
          /*task_start_tick=*/base::TimeTicks::Now()));
}

// Observes the Drive OnError event and add the related files to
// `error_syncing_files_`. The validation of a screencast happens in
// OnSyncingStatusUpdate because the drivefs::mojom::SyncingStatus contains the
// info about the file completed uploaded or not and other files status for the
// same screencast.
void PendingScreencastManager::OnError(
    const drivefs::mojom::DriveError& error) {
  base::FilePath error_file = base::FilePath(error.path);
  // mojom::DriveError::Type has 2 types: kCantUploadStorageFull and
  // kPinningFailedDiskFull. Only handle kCantUploadStorageFull so far.
  if (error.type != drivefs::mojom::DriveError::Type::kCantUploadStorageFull ||
      !IsWebmOrProjectorFile(error_file)) {
    return;
  }
  error_syncing_files_.insert(error_file);
}

const ash::PendingScreencastContainerSet&
PendingScreencastManager::GetPendingScreencasts() const {
  return pending_screencast_cache_;
}

void PendingScreencastManager::SetOnGetFileIdCallbackForTest(
    OnGetFileIdCallback callback) {
  on_get_file_id_callback_ = std::move(callback);
}

void PendingScreencastManager::SetOnGetRequestBodyCallbackForTest(
    OnGetRequestBodyCallback callback) {
  on_get_request_body_ = std::move(callback);
}

void PendingScreencastManager::SetProjectorXhrSenderForTest(
    std::unique_ptr<ash::ProjectorXhrSender> xhr_sender) {
  xhr_sender_ = std::move(xhr_sender);
}

void PendingScreencastManager::MaybeSwitchDriveFsObservation() {
  drive::DriveIntegrationService* const service =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  if (!service) {
    return;
  }

  drivefs::DriveFsHost* const host = service->GetDriveFsHost();
  if (!host || GetHost() == host) {
    return;
  }

  pending_screencast_cache_.clear();
  error_syncing_files_.clear();

  Observe(host);
}

void PendingScreencastManager::ToggleFileSyncingNotificationForPaths(
    const std::vector<base::FilePath>& paths,
    bool suppress) {
  auto* drivefs_integration =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  if (!drivefs_integration) {
    return;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  for (const auto& path : paths) {
    base::FilePath drive_path;
    drivefs_integration->GetRelativeDrivePath(path, &drive_path);
    if (suppress) {
      paths_notifications_suppressors_[drive_path] = std::make_unique<
          file_manager::ScopedSuppressDriveNotificationsForPath>(profile,
                                                                 drive_path);
    } else {
      paths_notifications_suppressors_.erase(drive_path);
    }
  }
}

void PendingScreencastManager::OnAppActiveStatusChanged(bool is_active) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  for (auto& [path, suppressor] : paths_notifications_suppressors_) {
    if (is_active) {
      if (!suppressor) {
        // Suppresses notification on app active.
        suppressor = std::make_unique<
            file_manager::ScopedSuppressDriveNotificationsForPath>(profile,
                                                                   path);
      }
    } else {
      // Resumes notification on app inactive.
      suppressor.reset();
    }
  }
}

void PendingScreencastManager::OnProcessAndGenerateNewScreencastsFinished(
    const base::TimeTicks task_start_tick,
    const ash::PendingScreencastContainerSet& screencasts) {
  const base::TimeTicks now = base::TimeTicks::Now();
  ash::RecordPendingScreencastBatchIOTaskDuration(now - task_start_tick);

  // Returns if pending screencasts didn't change.
  if (screencasts == pending_screencast_cache_) {
    return;
  }
  pending_screencast_cache_ = screencasts;

  // Notifies pending screencast status changed.
  pending_screencast_change_callback_.Run(pending_screencast_cache_);
  if (!last_pending_screencast_change_tick_.is_null()) {
    ash::RecordPendingScreencastChangeInterval(
        now - last_pending_screencast_change_tick_);
  }
  // Resets `last_pending_screencast_change_tick_` to null. We don't track time
  // delta between finish uploading and new uploading started.
  last_pending_screencast_change_tick_ =
      pending_screencast_cache_.empty() ? base::TimeTicks() : now;
}

void PendingScreencastManager::OnFileSyncedCompletely(
    const base::FilePath& event_file) {
  // Clean up the system notification suppression for `event_file`.
  paths_notifications_suppressors_.erase(event_file);

  // If observes a error uploaded file is now successfully uploaded, removes
  // it from `error_syncing_files_`:
  error_syncing_files_.erase(event_file);
  if (ash::features::IsProjectorUpdateIndexableTextEnabled()) {
    // If observes a ".projector" file is now successfully uploaded, updates
    // the indexable text and remove it from `syncing_metadata_files_`.
    const auto iter = syncing_metadata_files_.find(event_file);
    if (iter != syncing_metadata_files_.end()) {
      auto on_get_file_id_callback =
          on_get_file_id_callback_
              ? std::move(on_get_file_id_callback_)
              : base::BindOnce(&PendingScreencastManager::OnGetFileId,
                               weak_ptr_factory_.GetWeakPtr());

      // Posts a delayed task to get Drive metadata because the metadata might
      // not be polulated as the file get uploaded. This task has a long chain
      // of callbacks. The calling order is: GetDriveFileMetadata() ->
      // ParseFileIdOnGetMetaData() -> on_get_file_id_callback.
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&GetDriveFileMetadata, event_file,
                         std::move(on_get_file_id_callback)),
          kDriveGetMetadataDelay);
      syncing_metadata_files_.erase(iter);
    }
  }
}

void PendingScreencastManager::OnGetFileId(
    const base::FilePath& local_file_path,
    const std::string& file_id) {
  if (file_id.empty()) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BuildRequestBody, local_file_path),
      on_get_request_body_
          ? base::BindOnce(std::move(on_get_request_body_), file_id)
          : base::BindOnce(&PendingScreencastManager::SendDrivePatchRequest,
                           weak_ptr_factory_.GetWeakPtr(), file_id));
}

void PendingScreencastManager::SendDrivePatchRequest(
    const std::string& file_id,
    const std::string& request_body) {
  DCHECK(!file_id.empty());
  if (request_body.empty()) {
    return;
  }

  if (!xhr_sender_) {
    xhr_sender_ = std::make_unique<ash::ProjectorXhrSender>(
        ash::ProjectorAppClient::Get()->GetUrlLoaderFactory());
  }

  // TODO(b/288457397): Pass the primary account email after email become
  // required to send request with OAuth token.
  xhr_sender_->Send(
      GURL(base::StrCat({ash::kDriveV3BaseUrl, file_id})),
      ash::projector::mojom::RequestType::kPatch, request_body,
      /*use_credentials=*/false,
      /*use_api_key=*/false,
      base::BindOnce([](ash::projector::mojom::XhrResponsePtr xhr_response) {
        if (xhr_response->response_code !=
            ash::projector::mojom::XhrResponseCode::kSuccess) {
          LOG(ERROR) << "Failed to send Drive patch request for file."
                     << " Error: " << xhr_response->response_code;
        }
      }));
}
