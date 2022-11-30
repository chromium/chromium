// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_FILE_REMOVAL_STATUS_UPDATER_H_
#define CHROME_CHROME_CLEANER_OS_FILE_REMOVAL_STATUS_UPDATER_H_

#include <map>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"

namespace chrome_cleaner {

namespace internal {

// RemovalStatus update control utilities, exposed in the internal namespace so
// they can be accessed by tests.

// Indicates the action to be taken on RemovalStatus updates for files and
// folders.
enum RemovalStatusOverridePermission {
  // Ignore, this is expected and we shouldn't change the current value.
  // Example: updating removal status to NOT_FOUND after deleting the file.
  kSkip,
  // Override, this is an actual update and we should keep the most recent
  // value. Example: updating removal status to FAILED_TO_SCHEDULE_REMOVAL
  // when previous knowledge was FAILED_TO_REMOVE.
  kOkToOverride,
  // This should never happen in the code, and we should raise an error.
  // TODO(joenotcharles): Currently there is no error, and kNotAllowed is
  // implemented as kSkip. This is because DCHECK writes an error message to
  // the log, and until recently this took the logging lock which might already
  // be held while checking this permission. Now that it's safe to DCHECK while
  // the logging lock is held we should add a DCHECK.
  kNotAllowed,
};

// Maps pairs of RemovalStatus to the expected permission.
typedef std::map<RemovalStatus,
                 std::map<RemovalStatus, RemovalStatusOverridePermission>>
    RemovalStatusOverridePermissionMap;

// Returns the overriding map.
const RemovalStatusOverridePermissionMap&
GetRemovalStatusOverridePermissionMap();

}  // namespace internal

// This class manages a map of remove statuses for all files and folders
// encountered during cleaning, keyed by path. It does not distinguish whether
// the path refers to a file or a folder.
class FileRemovalStatusUpdater {
 public:
  struct FileRemovalStatus {
    // The full path that was passed to UpdateRemovalStatus or
    // UpdateQuarantineStatus. This is needed because when a file removal status
    // is logged, GetFileInformationProtoObject can be called, which needs a
    // full path that can be resolved.
    base::FilePath path;

    // The removal status of the last attempted update at the above path.
    RemovalStatus removal_status = REMOVAL_STATUS_UNSPECIFIED;

    // The quarantine status of the last attempted update at the above path.
    QuarantineStatus quarantine_status = QUARANTINE_STATUS_UNSPECIFIED;
  };

  typedef std::unordered_map<std::wstring, FileRemovalStatus>
      SanitizedPathToRemovalStatusMap;

  static FileRemovalStatusUpdater* GetInstance();

  virtual ~FileRemovalStatusUpdater();

  // Clears all saved removal statuses.
  void Clear();

  // Updates removal status for a file or folder given by |path|. Checks the
  // RemovalStatusOverridePermissionMap to see if the update is allowed, and
  // silently does nothing if the permission is kSkip.
  void UpdateRemovalStatus(const base::FilePath& path, RemovalStatus status);

  // Returns the removal status of |path|, or REMOVAL_STATUS_UNSPECIFIED if
  // the removal status have never been updated for that path.
  RemovalStatus GetRemovalStatus(const base::FilePath& path) const;

  // Returns the removal status of |sanitized_path|, or
  // REMOVAL_STATUS_UNSPECIFIED if the removal status have never
  // been updated for an unsanitized form of that path.
  RemovalStatus GetRemovalStatusOfSanitizedPath(
      const std::wstring& sanitized_path) const;

  // Updates quarantine status for a file given by |path|.
  // Note: UpdateRemovalStatus should be called for |path| at some point as
  // well, because it is invalid to quarantine a file that doesn't have some
  // removal status.
  void UpdateQuarantineStatus(const base::FilePath& path,
                              QuarantineStatus status);

  // Returns the quarantine status of |path|, or QUARANTINE_STATUS_UNSPECIFIED
  // if the quarantine status have never been updated for that path.
  QuarantineStatus GetQuarantineStatus(const base::FilePath& path) const;

  // Returns all saved removal statuses, keyed by sanitized path. Each
  // sanitized path is mapped to a single FileRemovalStatus which holds the
  // path and status values from the most recent call to UpdateRemovalStatus or
  // UpdateQuarantineStatus that had an effect.
  SanitizedPathToRemovalStatusMap GetAllRemovalStatuses() const;

 private:
  friend struct base::DefaultSingletonTraits<FileRemovalStatusUpdater>;

  FileRemovalStatusUpdater();

  // Locks access to |removal_statuses_|.
  mutable base::Lock removal_status_lock_;

  SanitizedPathToRemovalStatusMap removal_statuses_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_FILE_REMOVAL_STATUS_UPDATER_H_
