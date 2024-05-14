// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_path_reservation_tracker.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_item.h"
#include "components/filename_generation/filename_generation.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/download/internal/common/android/download_collection_bridge.h"
#endif

namespace download {

namespace {

// Identifier for a `DownloadItem` to scope the lifetime for references.
// `ReservationKey` is derived from `DownloadItem*`, used in comparison only,
// and are never deferenced.
using ReservationKey = std::uintptr_t;
typedef std::map<ReservationKey, base::FilePath> ReservationMap;

// The length of the suffix string we append for an intermediate file name.
// In the file name truncation, we keep the margin to append the suffix.
// TODO(kinaba): remove the margin. The user should be able to set maximum
// possible filename.
const size_t kIntermediateNameSuffixLength = sizeof(".crdownload") - 1;

#if BUILDFLAG(IS_WIN)
// On windows, zone identifier is appended to the downloaded file name during
// annotation. That increases the length of the final target path.
const size_t kZoneIdentifierLength = sizeof(":Zone.Identifier") - 1;
#endif  // BUILDFLAG(IS_WIN)

// Map of download path reservations. Each reserved path is associated with a
// ReservationKey=DownloadItem*. This object is destroyed in |Revoke()| when
// there are no more reservations.
ReservationMap* g_reservation_map = NULL;

base::LazyThreadPoolSequencedTaskRunner g_sequenced_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock()));

// Observes a DownloadItem for changes to its target path and state. Updates or
// revokes associated download path reservations as necessary. Created, invoked
// and destroyed on the UI thread.
class DownloadItemObserver : public DownloadItem::Observer,
                             public base::SupportsUserData::Data {
 public:
  explicit DownloadItemObserver(DownloadItem* download_item);

  DownloadItemObserver(const DownloadItemObserver&) = delete;
  DownloadItemObserver& operator=(const DownloadItemObserver&) = delete;

  ~DownloadItemObserver() override;

 private:
  // DownloadItem::Observer
  void OnDownloadUpdated(DownloadItem* download) override;
  void OnDownloadDestroyed(DownloadItem* download) override;

  raw_ptr<DownloadItem> download_item_;

  // Last known target path for the download.
  base::FilePath last_target_path_;

  static const int kUserDataKey;
};

// Returns true if the given path is in use by a path reservation,
// and has a different key than |item| if it is not null. Called on the task
// runner returned by DownloadPathReservationTracker::GetTaskRunner().
bool IsPathReservedInternal(const base::FilePath& path, ReservationKey item) {
  // No reservation map => no reservations.
  if (!g_reservation_map)
    return false;

  for (ReservationMap::const_iterator iter = g_reservation_map->begin();
       iter != g_reservation_map->end(); ++iter) {
    if ((!item || iter->first != item) &&
        base::FilePath::CompareEqualIgnoreCase(iter->second.value(),
                                               path.value()))
      return true;
  }
  return false;
}

// Returns true if the given path is in use by a path reservation,
// and has a different key than |item|. Called on the task
// runner returned by DownloadPathReservationTracker::GetTaskRunner().
bool IsAdditionalPathReserved(const base::FilePath& path, ReservationKey item) {
#if BUILDFLAG(IS_ANDROID)
  // If download collection is used, only file name needs to be
  // unique.
  if (DownloadCollectionBridge::ShouldPublishDownload(path)) {
    return IsPathReservedInternal(path.BaseName(), item);
  }
#endif  // BUILDFLAG(IS_ANDROID)  // No reservation map => no reservations.
  return IsPathReservedInternal(path, item);
}

// Returns true if the given path is in use by a path reservation.
bool IsPathReserved(const base::FilePath& path) {
  return IsAdditionalPathReserved(path, /*item=*/0);
}

// Returns true if the given path is in use by any path reservation or the
// file system. Called on the task runner returned by
// DownloadPathReservationTracker::GetTaskRunner().
bool IsPathInUse(const base::FilePath& path) {
  // If there is a reservation, then the path is in use.
  if (IsPathReserved(path))
    return true;

  // If the path exists in the file system, then the path is in use.
  if (base::PathExists(path))
    return true;

#if BUILDFLAG(IS_ANDROID)
  // If download collection is used, only file name needs to be
  // unique.
  if (DownloadCollectionBridge::ShouldPublishDownload(path))
    return DownloadCollectionBridge::FileNameExists(path.BaseName());
#endif
  return false;
}

// Create a unique filename by appending a uniquifier. Modifies |path| in place
// if successful and returns true. Otherwise |path| is left unmodified and
// returns false.
bool CreateUniqueFilename(int max_path_component_length,
                          const base::Time& download_start_time,
                          base::FilePath* path) {
  // Try every numeric uniquifier. Then make one attempt with the timestamp.
  for (int uniquifier = 1;
       uniquifier <= DownloadPathReservationTracker::kMaxUniqueFiles + 1;
       ++uniquifier) {
    // Append uniquifier.
    std::string suffix =
        (uniquifier > DownloadPathReservationTracker::kMaxUniqueFiles)
            ? base::UnlocalizedTimeFormatWithPattern(
                  download_start_time,
                  // ISO8601-compliant local timestamp suffix that avoids
                  // reserved characters that are forbidden on some OSes like
                  // Windows.
                  " - yyyy-MM-dd'T'HHmmss.SSS")
            : base::StringPrintf(" (%d)", uniquifier);

    base::FilePath path_to_check(*path);
    // If the name length limit is available (max_length != -1), and the
    // the current name exceeds the limit, truncate.
    if (max_path_component_length != -1) {
#if BUILDFLAG(IS_WIN)
      int limit =
          max_path_component_length -
          std::max(kIntermediateNameSuffixLength, kZoneIdentifierLength) -
          suffix.size();
#else
      int limit = max_path_component_length - kIntermediateNameSuffixLength -
                  suffix.size();
#endif  // BUILDFLAG(IS_WIN)
      // If truncation failed, give up uniquification.
      if (limit <= 0 ||
          !filename_generation::TruncateFilename(&path_to_check, limit))
        break;
    }
    path_to_check = path_to_check.InsertBeforeExtensionASCII(suffix);

    if (!IsPathInUse(path_to_check)) {
      *path = path_to_check;
      return true;
    }
  }

  return false;
}

struct CreateReservationInfo {
  ReservationKey key;
  base::FilePath source_path;
  base::FilePath suggested_path;
  base::FilePath default_download_path;
  base::FilePath temporary_path;
  base::FilePath fallback_directory;  // directory to use when target path
                                      // cannot be used.
  bool create_target_directory;
  base::Time start_time;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action;
};

// Check if |target_path| is writable.
bool IsPathWritable(const CreateReservationInfo& info,
                    const base::FilePath& target_path) {
  if (base::PathIsWritable(target_path.DirName()))
    return true;
  // If a temporary file is already created under the same dir as |target_path|,
  // return true. This is to avoid the windows network share issue. See
  // http://crbug.com/383765.
  return !info.temporary_path.empty() &&
         info.temporary_path.DirName() == target_path.DirName();
}

// Called when reservation conflicts happen. Returns the result on whether the
// conflict can be resolved, and uniquifying the file name if necessary.
PathValidationResult ResolveReservationConflicts(
    const CreateReservationInfo& info,
    int max_path_component_length,
    base::FilePath* target_path) {
  switch (info.conflict_action) {
    case DownloadPathReservationTracker::UNIQUIFY:
      return CreateUniqueFilename(max_path_component_length, info.start_time,
                                  target_path)
                 ? PathValidationResult::SUCCESS_RESOLVED_CONFLICT
                 : PathValidationResult::CONFLICT;

    case DownloadPathReservationTracker::OVERWRITE:
      if (IsPathReserved(*target_path))
        return PathValidationResult::CONFLICT;
      return PathValidationResult::SUCCESS;

    case DownloadPathReservationTracker::PROMPT:
      return PathValidationResult::CONFLICT;
  }
  NOTREACHED_IN_MIGRATION();
  return PathValidationResult::SUCCESS;
}

// Verify that |target_path| can be written to and also resolve any conflicts if
// necessary by uniquifying the filename.
PathValidationResult ValidatePathAndResolveConflicts(
    const CreateReservationInfo& info,
    base::FilePath* target_path) {
  // Check writability of the suggested path. If we can't write to it, use
  // the |default_download_path| if it is not empty or |fallback_directory|.
  // We'll prompt them in this case. No further amendments are made to the
  // filename since the user is going to be prompted.
  if (!IsPathWritable(info, *target_path)) {
    DVLOG(1) << "Unable to write to path \"" << target_path->value() << "\"";
    if (!info.default_download_path.empty() &&
        target_path->DirName() != info.default_download_path) {
      *target_path = info.default_download_path.Append(target_path->BaseName());
    } else {
      *target_path = info.fallback_directory.Append(target_path->BaseName());
    }
    return PathValidationResult::PATH_NOT_WRITABLE;
  }

  int max_path_component_length =
      base::GetMaximumPathComponentLength(target_path->DirName());
  // Check the limit of file name length if it could be obtained. When the
  // suggested name exceeds the limit, truncate or prompt the user.
  if (max_path_component_length != -1) {
#if BUILDFLAG(IS_WIN)
    int limit = max_path_component_length -
                std::max(kIntermediateNameSuffixLength, kZoneIdentifierLength);
#else
    int limit = max_path_component_length - kIntermediateNameSuffixLength;
#endif  // BUILDFLAG(IS_WIN)
    if (limit <= 0 ||
        !filename_generation::TruncateFilename(target_path, limit))
      return PathValidationResult::NAME_TOO_LONG;
  }

  // Disallow downloading a file onto itself. Assume that downloading a file
  // onto another file that differs only by case is not enough of a legitimate
  // edge case to justify determining the case sensitivity of the underlying
  // filesystem.
  if (*target_path == info.source_path)
    return PathValidationResult::SAME_AS_SOURCE;

  if (!IsPathInUse(*target_path))
    return PathValidationResult::SUCCESS;

  return ResolveReservationConflicts(info, max_path_component_length,
                                     target_path);
}

// Called on the task runner returned by
// DownloadPathReservationTracker::GetTaskRunner() to reserve a download path.
// This method:
// - Creates directory |default_download_path| if it doesn't exist.
// - Verifies that the parent directory of |suggested_path| exists and is
//   writeable.
// - Truncates the suggested name if it exceeds the filesystem's limit.
// - Uniquifies |suggested_path| if |should_uniquify_path| is true.
// - Schedules |callback| on the UI thread with the reserved path and a flag
//   indicating whether the returned path has been successfully verified.
// - Returns the result of creating the path reservation.
PathValidationResult CreateReservation(const CreateReservationInfo& info,
                                       base::FilePath* reserved_path) {
  // Create a reservation map if one doesn't exist. It will be automatically
  // deleted when all the reservations are revoked.
  if (g_reservation_map == NULL)
    g_reservation_map = new ReservationMap;

  // Erase the reservation if it already exists. This can happen during
  // automatic resumption where a new target determination request may be issued
  // for a DownloadItem without an intervening transition to INTERRUPTED.
  //
  // Revoking and re-acquiring the reservation forces us to re-verify the claims
  // we are making about the path.
  g_reservation_map->erase(info.key);

  base::FilePath target_path(info.suggested_path.NormalizePathSeparators());
  base::FilePath target_dir = target_path.DirName();
  base::FilePath filename = target_path.BaseName();

#if BUILDFLAG(IS_ANDROID)
  if (DownloadCollectionBridge::ShouldPublishDownload(target_path)) {
    PathValidationResult result = PathValidationResult::SUCCESS;
    // Disallow downloading a file onto itself. Assume that downloading a file
    if (target_path == info.source_path) {
      result = PathValidationResult::SAME_AS_SOURCE;
    } else if (IsPathInUse(target_path)) {
      // If the download is written to a content URI, put file name in the
      // reservation map as content URIs will always be different.
      int max_path_component_length =
          base::GetMaximumPathComponentLength(target_path.DirName());
      result = ResolveReservationConflicts(info, max_path_component_length,
                                           &target_path);
    }
    (*g_reservation_map)[info.key] = target_path.BaseName();
    *reserved_path = target_path;
    return result;
  }
#endif
  // Create target_dir if necessary and appropriate. target_dir may be the last
  // directory that the user selected in a FilePicker; if that directory has
  // since been removed, do NOT automatically re-create it. Only automatically
  // create the directory if it is the default Downloads directory or if the
  // caller explicitly requested automatic directory creation.
  if (!base::DirectoryExists(target_dir) &&
      (info.create_target_directory ||
       (!info.default_download_path.empty() &&
        (info.default_download_path == target_dir)))) {
    base::CreateDirectory(target_dir);
  }

  PathValidationResult result =
      ValidatePathAndResolveConflicts(info, &target_path);
  (*g_reservation_map)[info.key] = target_path;
  *reserved_path = target_path;
  return result;
}

// Called on a background thread to update the path of the reservation
// associated with |key| to |new_path|.
void UpdateReservation(ReservationKey key, const base::FilePath& new_path) {
  DCHECK(g_reservation_map != NULL);
  auto iter = g_reservation_map->find(key);
  if (iter != g_reservation_map->end()) {
    bool use_download_collection = false;
#if BUILDFLAG(IS_ANDROID)
    if (DownloadCollectionBridge::ShouldPublishDownload(new_path)) {
      use_download_collection = true;
      iter->second = new_path.BaseName();
    }
#endif  // BUILDFLAG(IS_ANDROID)
    if (!use_download_collection) {
      iter->second = new_path;
    }
  } else {
    // This would happen if an UpdateReservation() notification was scheduled on
    // the SequencedTaskRunner before ReserveInternal(), or after a Revoke()
    // call. Neither should happen.
    NOTREACHED_IN_MIGRATION();
  }
}

// Called on the FILE thread to remove the path reservation associated with
// |key|.
void RevokeReservation(ReservationKey key) {
  DCHECK(g_reservation_map != NULL);
  DCHECK(base::Contains(*g_reservation_map, key));
  g_reservation_map->erase(key);
  if (g_reservation_map->size() == 0) {
    // No more reservations. Delete map.
    delete g_reservation_map;
    g_reservation_map = NULL;
  }
}

void RunGetReservedPathCallback(
    DownloadPathReservationTracker::ReservedPathCallback callback,
    const base::FilePath* reserved_path,
    PathValidationResult result) {
  std::move(callback).Run(result, *reserved_path);
}

// Gets the path reserved in the global |g_reservation_map|. For content Uri,
// file name instead of file path is used.
base::FilePath GetReservationPath(DownloadItem* download_item) {
#if BUILDFLAG(IS_ANDROID)
  if (download_item->GetTargetFilePath().IsContentUri())
    return download_item->GetFileNameToReportUser();
#endif
  return download_item->GetTargetFilePath();
}

DownloadItemObserver::DownloadItemObserver(DownloadItem* download_item)
    : download_item_(download_item),
      last_target_path_(GetReservationPath(download_item)) {
  // Deregister the old observer from |download_item_| if there is one.
  DownloadItemObserver* observer = static_cast<DownloadItemObserver*>(
      download_item_->GetUserData(&kUserDataKey));
  if (observer)
    download_item_->RemoveObserver(observer);
  download_item_->AddObserver(this);
  download_item_->SetUserData(&kUserDataKey, base::WrapUnique(this));
}

DownloadItemObserver::~DownloadItemObserver() = default;

void DownloadItemObserver::OnDownloadUpdated(DownloadItem* download) {
  switch (download->GetState()) {
    case DownloadItem::IN_PROGRESS: {
      // Update the reservation.
      base::FilePath new_target_path = GetReservationPath(download);
      if (new_target_path != last_target_path_) {
        DownloadPathReservationTracker::GetTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(&UpdateReservation,
                           reinterpret_cast<ReservationKey>(download),
                           new_target_path));
        last_target_path_ = new_target_path;
      }
      break;
    }

    case DownloadItem::COMPLETE:
      // If the download is complete, then it has already been renamed to the
      // final name. The existence of the file on disk is sufficient to prevent
      // conflicts from now on.

    case DownloadItem::CANCELLED:
      // We no longer need the reservation if the download is being removed.

    case DownloadItem::INTERRUPTED:
      // The download filename will need to be re-generated when the download is
      // restarted. Holding on to the reservation now would prevent the name
      // from being used for a subsequent retry attempt.
      DownloadPathReservationTracker::GetTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&RevokeReservation,
                         reinterpret_cast<ReservationKey>(download)));
      download->RemoveObserver(this);
      download->RemoveUserData(&kUserDataKey);
      break;

    case DownloadItem::MAX_DOWNLOAD_STATE:
      // Compiler appeasement.
      NOTREACHED_IN_MIGRATION();
  }
}

void DownloadItemObserver::OnDownloadDestroyed(DownloadItem* download) {
  // Items should be COMPLETE/INTERRUPTED/CANCELLED before being destroyed.
  NOTREACHED_IN_MIGRATION();
  DownloadPathReservationTracker::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&RevokeReservation,
                                reinterpret_cast<ReservationKey>(download)));
}

// static
const int DownloadItemObserver::kUserDataKey = 0;

}  // namespace

// static
void DownloadPathReservationTracker::GetReservedPath(
    DownloadItem* download_item,
    const base::FilePath& target_path,
    const base::FilePath& default_path,
    const base::FilePath& fallback_directory,
    bool create_directory,
    FilenameConflictAction conflict_action,
    ReservedPathCallback callback) {
  // Attach an observer to the download item so that we know when the target
  // path changes and/or the download is no longer active.
  new DownloadItemObserver(download_item);
  // DownloadItemObserver deletes itself.

  base::FilePath* reserved_path = new base::FilePath;
  base::FilePath source_path;
  if (download_item->GetURL().SchemeIsFile())
    net::FileURLToFilePath(download_item->GetURL(), &source_path);
  CreateReservationInfo info = {reinterpret_cast<ReservationKey>(download_item),
                                source_path,
                                target_path,
                                default_path,
                                download_item->GetTemporaryFilePath(),
                                fallback_directory,
                                create_directory,
                                download_item->GetStartTime(),
                                conflict_action};

  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateReservation, info, reserved_path),
      base::BindOnce(&RunGetReservedPathCallback, std::move(callback),
                     base::Owned(reserved_path)));
}

// static
bool DownloadPathReservationTracker::IsPathInUseForTesting(
    const base::FilePath& path) {
  return IsPathInUse(path);
}

// static
scoped_refptr<base::SequencedTaskRunner>
DownloadPathReservationTracker::GetTaskRunner() {
  return g_sequenced_task_runner.Get();
}

// static
void DownloadPathReservationTracker::CheckDownloadPathForExistingDownload(
    const base::FilePath& target_path,
    DownloadItem* download_item,
    CheckDownloadPathCallback callback) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&IsAdditionalPathReserved, target_path,
                     reinterpret_cast<ReservationKey>(download_item)),
      std::move(callback));
}

}  // namespace download
