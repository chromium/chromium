// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CLIENT_ID_BACKUP_FILE_MANAGER_H_
#define CHROME_INSTALLER_UTIL_CLIENT_ID_BACKUP_FILE_MANAGER_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"

class GoogleUpdateSettings;

// Singleton keeping track of the client ID backup file, which is used to
// store the client ID backup and indicate the user's metrics reporting
// choice (UMA). Note that this class does not handle the three-state
// setting managed by MetricsReportingChoiceService.
// Uses chrome::DIR_USER_DATA as a location to store the file.
// It uses an internal cache to avoid extra IO. The behavior of the
// cache is documented in relevant methods.
// The cache value can be different than the disk content if the
// IO fails. This is done to prefer applying changes to the current
// session even if persistence fails.
// This class is thread-safe.
class ClientIdBackupFileManager {
 public:
  static ClientIdBackupFileManager& GetInstance();

  static base::FilePath GetBackupFilePathForTesting();

  // This is a singleton, no copy constructors.
  ClientIdBackupFileManager(const ClientIdBackupFileManager&) = delete;
  ClientIdBackupFileManager& operator=(const ClientIdBackupFileManager&) =
      delete;

  // Returns the client ID, reading it from the file on disk if needed.
  // Must always be called before any calls to ClientIdFromCache().
  // The returned value might be an empty string if the client agreed
  // to collect data but didn't have the client_id assigned yet.
  // Returns std::nullopt if metrics should not be reported, which may be
  // caused by a read error or lack of available backup which indicates
  // that a client didn't agree to collect data.
  std::optional<std::string> ClientIdFromCacheOrDisk();

  // Returns the client ID that is cached.
  // The returned value might be an empty string if the client agreed
  // to collect data but didn't have the client_id assigned yet.
  // Returns std::nullopt when called before the first
  // ClientIdFromCacheOrDisk() finishes its work or if the metrics
  // should not be reported.
  std::optional<std::string> ClientIdFromCache() const;

  // Sets the client ID in memory and on disk to the same value.
  // Calls to ClientIdFromCache() done during the runtime of this
  // method can return either the previous or the new value.
  // Returns false but updates the cached value if the write
  // to disk fails.
  bool SetClientId(base::PassKey<GoogleUpdateSettings>, std::string client_id);

  // Clears the client ID in memory and on disk.
  bool ClearClientId(base::PassKey<GoogleUpdateSettings>);

  // Overload for testing that doesn't require a PassKey.
  bool ClearClientIdForTesting();

  // Overload for testing that doesn't require a PassKey.
  bool SetClientIdForTesting(std::string client_id);

  // Resets the internal state of the singleton.
  void ResetForTesting();

 private:
  bool SetClientIdInternal(std::optional<std::string> client_id);

  friend class base::NoDestructor<ClientIdBackupFileManager>;

  ClientIdBackupFileManager();
  ~ClientIdBackupFileManager();

  // The in-memory version of the client ID.
  // It's initially not set until the first successful load from disk.
  // After it is set, it's returned assuming that it didn't change on disk.
  // The cache persists for the lifetime of the application, but will be
  // updated with an explicit call from the user that also updates it on disk.
  // Note: This value can be an empty string to indicate permission to
  // collect metrics without storing the actual client ID yet.
  mutable base::Lock client_id_lock_;
  std::optional<std::string> client_id_ GUARDED_BY(client_id_lock_);
};

#endif  // CHROME_INSTALLER_UTIL_CLIENT_ID_BACKUP_FILE_MANAGER_H_
