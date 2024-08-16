// Copyright 2012 The Chromium Authors
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

// A SyncChange object reflects a change to a sync entity (unit of sync data),
// which can be either a delete, add, or an update. Specifically, it is used
// in the SyncableService API, as opposed to the analogous class EntityChange
// used in the more modern equivalent DataTypeSyncBridge API.
//
// Note: it is safe and cheap to pass these by value or make copies, as they do
// not create deep copies of their internal data.
class SyncChange {
 public:
  enum SyncChangeType {
    ACTION_ADD,
    ACTION_UPDATE,
    ACTION_DELETE,
  };

  // Returns a string representation of |change_type|.
  static std::string ChangeTypeToString(SyncChangeType change_type);

  // Create a new change with the specified sync data. |sync_data| must be
  // valid.
  SyncChange(const base::Location& from_here,
             SyncChangeType change_type,
             const SyncData& sync_data);

  // Copyable cheaply.
  SyncChange(const SyncChange&) = default;

  ~SyncChange();

  SyncChange& operator=(const SyncChange&) = default;

  // Getters.
  SyncChangeType change_type() const { return change_type_; }
  const SyncData& sync_data() const { return sync_data_; }
  base::Location location() const { return location_; }

  // Returns a string representation of the entire object. Used for gmock
  // printing method, PrintTo.
  std::string ToString() const;

 private:
  base::Location location_;
  SyncChangeType change_type_;
  SyncData sync_data_;
};

// gmock printer helper.
void PrintTo(const SyncChange& sync_change, std::ostream* os);

using SyncChangeList = std::vector<SyncChange>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_CHANGE_H_
