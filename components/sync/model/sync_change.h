// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_CHANGE_H_
#define COMPONENTS_SYNC_MODEL_SYNC_CHANGE_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/location.h"
#include "components/sync/model/sync_data.h"

namespace syncer {

// A SyncChange object reflects a change to a piece of synced data. The change
// can be either a delete, add, or an update. All data relevant to the change
// is encapsulated within the SyncChange, which, once created, is immutable.
// Note: it is safe and cheap to pass these by value or make copies, as they do
// not create deep copies of their internal data.
class SyncChange {
 public:
  enum SyncChangeType {
    ACTION_INVALID,
    ACTION_ADD,
    ACTION_UPDATE,
    ACTION_DELETE,
  };

  // Default constructor creates an invalid change.
  SyncChange();
  // Create a new change with the specified sync data.
  SyncChange(const base::Location& from_here,
             SyncChangeType change_type,
             const SyncData& sync_data);
  ~SyncChange();

  // Copy constructor and assignment operator welcome.

  // Whether this change is valid. This must be true before attempting to access
  // the data.
  // Deletes: Requires valid tag when going to the syncer. Requires valid
  //          specifics when coming from the syncer.
  // Adds, Updates: Require valid tag and specifics when going to the syncer.
  //                Require only valid specifics when coming from the syncer.
  bool IsValid() const;

  // Getters.
  SyncChangeType change_type() const;
  SyncData sync_data() const;
  base::Location location() const;

  // Returns a string representation of |change_type|.
  static std::string ChangeTypeToString(SyncChangeType change_type);

  // Returns a string representation of the entire object. Used for gmock
  // printing method, PrintTo.
  std::string ToString() const;

 private:
  base::Location location_;

  SyncChangeType change_type_;

  // An immutable container for the data of this SyncChange. Whenever
  // SyncChanges are copied, they copy references to this data.
  SyncData sync_data_;
};

// gmock printer helper.
void PrintTo(const SyncChange& sync_change, std::ostream* os);

using SyncChangeList = std::vector<SyncChange>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_CHANGE_H_
