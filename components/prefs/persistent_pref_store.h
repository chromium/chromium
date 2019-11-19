// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PERSISTENT_PREF_STORE_H_
#define COMPONENTS_PREFS_PERSISTENT_PREF_STORE_H_

#include <string>

#include "base/callback.h"
#include "components/prefs/prefs_export.h"
#include "components/prefs/writeable_pref_store.h"

// This interface is complementary to the PrefStore interface, declaring
// additional functionality that adds support for setting values and persisting
// the data to some backing store.
class COMPONENTS_PREFS_EXPORT PersistentPrefStore : public WriteablePrefStore {
 public:
  // Unique integer code for each type of error so we can report them
  // distinctly in a histogram.
  // NOTE: Don't change the explicit values of the enums as it will change the
  // server's meaning of the histogram.
  enum PrefReadError {
    PREF_READ_ERROR_NONE = 0,
    PREF_READ_ERROR_JSON_PARSE = 1,
    PREF_READ_ERROR_JSON_TYPE = 2,
    PREF_READ_ERROR_ACCESS_DENIED = 3,
    PREF_READ_ERROR_FILE_OTHER = 4,
    PREF_READ_ERROR_FILE_LOCKED = 5,
    PREF_READ_ERROR_NO_FILE = 6,
    PREF_READ_ERROR_JSON_REPEAT = 7,
    // PREF_READ_ERROR_OTHER = 8,  // Deprecated.
    PREF_READ_ERROR_FILE_NOT_SPECIFIED = 9,
    // Indicates that ReadPrefs() couldn't complete synchronously and is waiting
    // for an asynchronous task to complete first.
    PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE = 10,
    PREF_READ_ERROR_MAX_ENUM
  };

  class ReadErrorDelegate {
   public:
    virtual ~ReadErrorDelegate() {}

    virtual void OnError(PrefReadError error) = 0;
  };

  // Whether the store is in a pseudo-read-only mode where changes are not
  // actually persisted to disk.  This happens in some cases when there are
  // read errors during startup.
  virtual bool ReadOnly() const = 0;

  // Gets the read error. Only valid if IsInitializationComplete() returns true.
  virtual PrefReadError GetReadError() const = 0;

  // Reads the preferences from disk. Notifies observers via
  // "PrefStore::OnInitializationCompleted" when done.
  virtual PrefReadError ReadPrefs() = 0;

  // Reads the preferences from disk asynchronously. Notifies observers via
  // "PrefStore::OnInitializationCompleted" when done. Also it fires
  // |error_delegate| if it is not NULL and reading error has occurred.
  // Owns |error_delegate|.
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) = 0;

  // Lands pending writes to disk. |reply_callback| will be posted to the
  // current sequence when changes have been written.
  // |synchronous_done_callback| on the other hand will be invoked right away
  // wherever the writes complete (could even be invoked synchronously if no
  // writes need to occur); this is useful when the current thread cannot pump
  // messages to observe the reply (e.g. nested loops banned on main thread
  // during shutdown). |synchronous_done_callback| must be thread-safe.
  virtual void CommitPendingWrite(
      base::OnceClosure reply_callback = base::OnceClosure(),
      base::OnceClosure synchronous_done_callback = base::OnceClosure());

  // Schedule a write if there is any lossy data pending. Unlike
  // CommitPendingWrite() this does not immediately sync to disk, instead it
  // triggers an eventual write if there is lossy data pending and if there
  // isn't one scheduled already.
  virtual void SchedulePendingLossyWrites() = 0;

  // It should be called only for Incognito pref store.
  virtual void ClearMutableValues() = 0;

  // Cleans preference data that may have been saved outside of the store.
  virtual void OnStoreDeletionFromDisk() = 0;

  // TODO(crbug.com/942491) Remove this after fixing the bug.
  virtual bool IsInMemoryPrefStore() const;

 protected:
  ~PersistentPrefStore() override {}
};

#endif  // COMPONENTS_PREFS_PERSISTENT_PREF_STORE_H_
