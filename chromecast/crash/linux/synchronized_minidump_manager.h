// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_
#define CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "chromecast/crash/linux/dump_info.h"

namespace chromecast {

// Abstract base class for mutually-exclusive minidump handling. Ensures
// synchronized access among instances of this class to the minidumps directory
// using a file lock. The "lockfile" also holds serialized metadata about each
// of the minidumps in the directory. Derived classes should not access the
// lockfile directly. Instead, use protected methods to query and modify the
// metadata, but only within the implementation of DoWork().
//
// This class holds an in memory representation of the lockfile and metadata
// file when the lockfile is held. Modifier methods work on this in memory
// representation. When the lockfile is released, the in memory representations
// are written to file
//
// The lockfile file is of the following format:
// { <dump_info1> }
// { <dump_info2> }
// ...
// { <dump_infoN> }
//
// Note that this isn't a valid json object. It is formatted in this way so
// that producers to this file do not need to understand json.
//
// Current external producers:
// + watchdog
//
//
// The metadata file is a separate file containing a json dictionary.
//
class SynchronizedMinidumpManager {
 public:
  // Length of a ratelimit period in seconds.
  static const int kRatelimitPeriodSeconds;

  // Number of dumps allowed per period.
  static const int kRatelimitPeriodMaxDumps;

  SynchronizedMinidumpManager(const SynchronizedMinidumpManager&) = delete;
  SynchronizedMinidumpManager& operator=(const SynchronizedMinidumpManager&) =
      delete;

  virtual ~SynchronizedMinidumpManager();

 protected:
  SynchronizedMinidumpManager();

  // Acquires the lock, calls DoWork(), then releases the lock when DoWork()
  // returns. Derived classes should expose a method which calls this. Returns
  // the status of DoWork(), or false if the lock was not successfully acquired.
  bool AcquireLockAndDoWork();

  // Derived classes must implement this method. It will be called from
  // DoWorkLocked after the lock has been successfully acquired. The lockfile
  // shall be accessed and mutated only through the methods below. All other
  // files shall be managed as needed by the derived class.
  virtual bool DoWork() = 0;

  // Get the current dumps in the lockfile.
  std::vector<std::unique_ptr<DumpInfo>> GetDumps();

  // Set |dumps| as the dumps in |lockfile_|, replacing current list of dumps.
  bool SetCurrentDumps(const std::vector<std::unique_ptr<DumpInfo>>& dumps);

  // Serialize |dump_info| and append it to the lockfile. Note that the child
  // class must only call this inside DoWork(). This should be the only method
  // used to write to the lockfile. Only call this if the minidump has been
  // generated in the minidumps directory successfully. Returns true on success,
  // false otherwise.
  bool AddEntryToLockFile(const DumpInfo& dump_info);

  // Remove the lockfile entry at |index| in the current in memory
  // representation of the lockfile. If the index is invalid returns false,
  // otherwise returns true.
  bool RemoveEntryFromLockFile(int index);

  // Get the number of un-uploaded dumps in the dump_path directory.
  // If delete_all_dumps is true, also delete all these files, this is used to
  // clean lingering dump files.
  int GetNumDumps(bool delete_all_dumps);

  // Increment the number of dumps in the current ratelimit period.
  // Returns true on success, false on error.
  bool IncrementNumDumpsInCurrentPeriod();

  // Decrement the number of dumps in the current ratelimit period.
  // Returns true on success, false on error.
  bool DecrementNumDumpsInCurrentPeriod();

  // Start a new rate-limit period, thus allowing crash uploads to proceed.
  void ResetRateLimitPeriod();

  // Returns true when dumps uploaded in current rate limit period is less than
  // |kRatelimitPeriodMaxDumps|. Resets rate limit period if period time has
  // elapsed.
  bool CanUploadDump();

  // Returns true when there are dumps in the lockfile or extra files in the
  // dump directory, false otherwise.
  // Used to avoid unnecessary file locks in consumers.
  bool HasDumps();

  // Ensures that the lockfile and metadata are in a valid state. This requires
  // obtaining the lockfile. Will fail if lockfile already held.
  bool InitializeFileState();

  // Cached path for the minidumps directory.
  const base::FilePath dump_path_;

 private:
  // Acquire the lock file. Blocks if another process holds it, or if called
  // a second time by the same process. Returns true if successful, or false
  // otherwise.
  bool AcquireLockFile();

  // Parse the lockfile and metadata file, populating |dumps_| and |metadata_|
  // for modifier functions to use. Returns false if an error occurred,
  // otherwise returns true. This must not be called unless |this| has acquired
  // the lock.
  bool ParseFiles();

  // Write deserialized |dumps| to |lockfile_path_| and the deserialized
  // |metadata| to |metadata_path_|.
  bool WriteFiles(const base::Value::List& dumps,
                  const base::Value::Dict& metadata);

  // Creates an empty lock file and an initialized metadata file.
  bool InitializeFiles();

  // Release the lock file with the associated *fd*.
  void ReleaseLockFile();

  const base::FilePath lockfile_path_;
  const base::FilePath metadata_path_;
  int lockfile_fd_;
  std::optional<base::Value::Dict> metadata_;
  std::optional<base::Value::List> dumps_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_
