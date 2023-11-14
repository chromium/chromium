// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_EXTENSIONS_ACTIVITY_H_
#define COMPONENTS_SYNC_BASE_EXTENSIONS_ACTIVITY_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace syncer {

// A storage to record usage of extensions APIs to send to sync
// servers, with the ability to purge data once sync servers have
// acknowledged it (successful commit response).
class ExtensionsActivity
    : public base::RefCountedThreadSafe<ExtensionsActivity> {
 public:
  // A data record of activity performed by extension |extension_id|.
  struct Record {
    // The human-readable ID identifying the extension responsible
    // for the activity reported in this Record.
    std::string extension_id;

    // How many times the extension successfully invoked a write
    // operation through the bookmarks API since the last CommitMessage.
    uint32_t bookmark_write_count = 0U;
  };

  using Records = std::map<std::string, Record>;

  ExtensionsActivity();

  // Fill |buffer| with all current records and then clear the
  // internal records. Called on sync thread to append records to sync commit
  // message.
  void GetAndClearRecords(Records* buffer);

  // Merge |records| with the current set of records. Called on sync thread to
  // put back records if sync commit failed.
  void PutRecords(const Records& records);

  // Increment write count of the specified extension.
  void UpdateRecord(const std::string& extension_id);

 private:
  friend class base::RefCountedThreadSafe<ExtensionsActivity>;
  ~ExtensionsActivity();

  Records records_;
  mutable base::Lock records_lock_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_EXTENSIONS_ACTIVITY_H_
