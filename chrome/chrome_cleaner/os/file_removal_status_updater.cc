// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"

#include "chrome/chrome_cleaner/os/file_path_sanitization.h"

namespace chrome_cleaner {

namespace internal {

namespace {

// Returns true if the |previous_removal_status| can be overriden by
// |new_removal_status| for a matched file/folder, according to the rules
// defined by GetRemovalStatusOverridePermissionMap().
bool RemovalStatusCanBeOverriddenBy(RemovalStatus previous_removal_status,
                                    RemovalStatus new_removal_status) {
  const RemovalStatusOverridePermissionMap& decisions_map =
      GetRemovalStatusOverridePermissionMap();
  auto it = decisions_map.find(previous_removal_status);
  if (it == decisions_map.end())
    return false;

  auto it2 = it->second.find(new_removal_status);
  if (it2 == it->second.end())
    return false;

  return it2->second == kOkToOverride;
}

}  // namespace

const RemovalStatusOverridePermissionMap&
GetRemovalStatusOverridePermissionMap() {
  static const RemovalStatusOverridePermissionMap* overriding_decisions = []() {
    RemovalStatusOverridePermissionMap* overriding_decisions =
        new RemovalStatusOverridePermissionMap();

    // This mapping can also be viewed in the following spreadsheet:
    // http://go/chrome-cleaner-removal-status-overrides
    (*overriding_decisions)[REMOVAL_STATUS_UNSPECIFIED] = {
        {REMOVAL_STATUS_UNSPECIFIED, kOkToOverride},
        {REMOVAL_STATUS_MATCHED_ONLY, kOkToOverride},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_NOT_FOUND, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kOkToOverride},
    };

    (*overriding_decisions)[REMOVAL_STATUS_MATCHED_ONLY] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kOkToOverride},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_NOT_FOUND, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kOkToOverride},
    };

    (*overriding_decisions)[REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kNotAllowed},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_REMOVED, kNotAllowed},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kNotAllowed},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_NOT_FOUND, kNotAllowed},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kNotAllowed},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kNotAllowed},
    };

    (*overriding_decisions)[REMOVAL_STATUS_REMOVED] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kNotAllowed},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_NOT_FOUND, kSkip},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kNotAllowed},
    };

    (*overriding_decisions)[REMOVAL_STATUS_FAILED_TO_REMOVE] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kNotAllowed},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_NOT_FOUND, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kOkToOverride},
    };

    (*overriding_decisions)[REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kNotAllowed},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kSkip},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kSkip},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kSkip},
        {REMOVAL_STATUS_NOT_FOUND, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kSkip},
    };

    (*overriding_decisions)[REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kNotAllowed},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_NOT_FOUND, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kOkToOverride},
    };

    (*overriding_decisions)[REMOVAL_STATUS_NOT_FOUND] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kNotAllowed},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_NOT_FOUND, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kOkToOverride},
    };

    (*overriding_decisions)[REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kNotAllowed},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kSkip},
        {REMOVAL_STATUS_NOT_FOUND, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kSkip},
    };

    (*overriding_decisions)[REMOVAL_STATUS_ERROR_IN_ARCHIVER] = {
        {REMOVAL_STATUS_UNSPECIFIED, kNotAllowed},
        {REMOVAL_STATUS_MATCHED_ONLY, kNotAllowed},
        {REMOVAL_STATUS_BLACKLISTED_FOR_REMOVAL, kNotAllowed},
        {REMOVAL_STATUS_REMOVED, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_REMOVE, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_FAILED_TO_SCHEDULE_FOR_REMOVAL, kOkToOverride},
        {REMOVAL_STATUS_NOT_FOUND, kOkToOverride},
        {REMOVAL_STATUS_SCHEDULED_FOR_REMOVAL_FALLBACK, kOkToOverride},
        {REMOVAL_STATUS_ERROR_IN_ARCHIVER, kOkToOverride},
    };
    return overriding_decisions;
  }();

  return *overriding_decisions;
}

}  // namespace internal

// static
FileRemovalStatusUpdater* FileRemovalStatusUpdater::GetInstance() {
  return base::Singleton<FileRemovalStatusUpdater>::get();
}

FileRemovalStatusUpdater::~FileRemovalStatusUpdater() = default;

void FileRemovalStatusUpdater::Clear() {
  base::AutoLock lock(removal_status_lock_);
  removal_statuses_.clear();
}

void FileRemovalStatusUpdater::UpdateRemovalStatus(const base::FilePath& path,
                                                   RemovalStatus status) {
  // Compare against the highest known removal status, not RemovalStatus_MAX.
  // That way if the RemovalStatus enum changes, a unit test that iterates up
  // to RemovalStatus_MAX will fail on this DCHECK. This is a reminder to add
  // the new RemovalStatus to RemovalStatusCanBeOverriddenBy().
  DCHECK(status > REMOVAL_STATUS_UNSPECIFIED &&
         status <= REMOVAL_STATUS_ERROR_IN_ARCHIVER)
      << "Unknown RemovalStatus: need to update "
         "RemovalStatusCanBeOverriddenBy()?";

  const base::string16 sanitized_path = SanitizePath(path);

  base::AutoLock lock(removal_status_lock_);

  auto it = removal_statuses_.find(sanitized_path);
  if (it == removal_statuses_.end()) {
    FileRemovalStatus new_status;
    new_status.path = path;
    new_status.removal_status = status;
    new_status.quarantine_status = QUARANTINE_STATUS_UNSPECIFIED;
    removal_statuses_.emplace(sanitized_path, new_status);
  } else {
    // Only update the entry if the new status is allowed to override the
    // current status.
    if (internal::RemovalStatusCanBeOverriddenBy(it->second.removal_status,
                                                 status)) {
      it->second.path = path;
      it->second.removal_status = status;
    }
  }
}

RemovalStatus FileRemovalStatusUpdater::GetRemovalStatus(
    const base::FilePath& path) const {
  return GetRemovalStatusOfSanitizedPath(SanitizePath(path));
}

RemovalStatus FileRemovalStatusUpdater::GetRemovalStatusOfSanitizedPath(
    const base::string16& sanitized_path) const {
  base::AutoLock lock(removal_status_lock_);
  const auto it = removal_statuses_.find(sanitized_path);
  return it == removal_statuses_.end() ? REMOVAL_STATUS_UNSPECIFIED
                                       : it->second.removal_status;
}

void FileRemovalStatusUpdater::UpdateQuarantineStatus(
    const base::FilePath& path,
    QuarantineStatus status) {
  // QUARANTINE_STATUS_UNSPECIFIED should never be set.
  DCHECK(status > QUARANTINE_STATUS_UNSPECIFIED &&
         status <= QuarantineStatus_MAX);

  const base::string16 sanitized_path = SanitizePath(path);

  base::AutoLock lock(removal_status_lock_);

  auto it = removal_statuses_.find(sanitized_path);
  // If the |sanitized_path| is not found, it will initialize the removal status
  // with |REMOVAL_STATUS_UNSPECIFIED|, which should be updated with other valid
  // statuses later.
  if (it == removal_statuses_.end()) {
    FileRemovalStatus new_status;
    new_status.path = path;
    new_status.removal_status = REMOVAL_STATUS_UNSPECIFIED;
    new_status.quarantine_status = status;
    removal_statuses_.emplace(sanitized_path, new_status);
  } else {
    it->second.path = path;
    it->second.quarantine_status = status;
  }
}

QuarantineStatus FileRemovalStatusUpdater::GetQuarantineStatus(
    const base::FilePath& path) const {
  const base::string16 sanitized_path = SanitizePath(path);

  base::AutoLock lock(removal_status_lock_);

  const auto it = removal_statuses_.find(sanitized_path);
  return it == removal_statuses_.end() ? QUARANTINE_STATUS_UNSPECIFIED
                                       : it->second.quarantine_status;
}

FileRemovalStatusUpdater::SanitizedPathToRemovalStatusMap
FileRemovalStatusUpdater::GetAllRemovalStatuses() const {
  base::AutoLock lock(removal_status_lock_);
  // Returns a copy of the map.
  return removal_statuses_;
}

FileRemovalStatusUpdater::FileRemovalStatusUpdater() = default;

}  // namespace chrome_cleaner
