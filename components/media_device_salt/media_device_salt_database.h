// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_DATABASE_H_
#define COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_DATABASE_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "content/public/browser/storage_partition.h"
#include "sql/database.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace media_device_salt {

// Utility to create a random salt.
std::string CreateRandomSalt();

// Helper class that encapsulates the database logic for storing media device
// salts. These salts are used to generate persistent media device IDs used by
// APIs such as getUserMedia(), enumerateDevices() and some extension APIs.
//
// This class must be constructed and used on a sequence that allows blocking.
class MediaDeviceSaltDatabase {
 public:
  // The database will be in-memory if `db_path` is empty.
  explicit MediaDeviceSaltDatabase(const base::FilePath& db_path);
  MediaDeviceSaltDatabase(const MediaDeviceSaltDatabase&) = delete;
  MediaDeviceSaltDatabase& operator=(const MediaDeviceSaltDatabase&) = delete;

  // Tries to retrieve the salt corresponding to the given `storage_key`.
  // If found, the salt is returned.
  // If `storage_key` is not found, a new salt is inserted for `storage_key` and
  // its value is returned. If `candidate_salt` is given, it will be the new
  // inserted salt; otherwise, the new salt will be a random salt.
  // If there is a database error or if `storage_key` is not serializable,
  // nullopt is returned.
  std::optional<std::string> GetOrInsertSalt(
      const blink::StorageKey& storage_key,
      std::optional<std::string> candidate_salt = std::nullopt);

  // Deletes entries in the given time range whose storage keys match the given
  // `matcher`. If `matcher` is null, all keys are assumed to match.
  void DeleteEntries(
      base::Time delete_begin,
      base::Time delete_end,
      content::StoragePartition::StorageKeyMatcherFunction matcher =
          content::StoragePartition::StorageKeyMatcherFunction());

  // Deletes the entry with the given `storage_key`.
  void DeleteEntry(const blink::StorageKey& storage_key);

  // Returns all storage keys for which there is an entry.
  std::vector<blink::StorageKey> GetAllStorageKeys();

  void ForceErrorForTesting() { force_error_ = true; }
  sql::Database& DatabaseForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return db_;
  }

 private:
  // Returns true if the database is successfully initialized, false otherwise.
  bool EnsureOpen(bool is_retry = false);

  // Error callback.
  void OnDatabaseError(int error, sql::Statement* statement);

  SEQUENCE_CHECKER(sequence_checker_);
  const base::FilePath db_path_;  // Empty if using in-memory database.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool force_error_ = false;
};

}  // namespace media_device_salt

#endif  // COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_DATABASE_H_
