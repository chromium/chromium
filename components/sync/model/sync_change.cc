// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_change.h"

#include <ostream>

#include "base/check.h"
#include "base/notreached.h"

namespace syncer {

// static
std::string SyncChange::ChangeTypeToString(SyncChangeType change_type) {
  switch (change_type) {
    case ACTION_ADD:
      return "ACTION_ADD";
    case ACTION_UPDATE:
      return "ACTION_UPDATE";
    case ACTION_DELETE:
      return "ACTION_DELETE";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

SyncChange::SyncChange(const base::Location& from_here,
                       SyncChangeType change_type,
                       const SyncData& sync_data)
    : location_(from_here), change_type_(change_type), sync_data_(sync_data) {
  DCHECK(sync_data.IsValid()) << " from " << from_here.ToString();
  DCHECK(!sync_data.GetClientTagHash().value().empty());
}

SyncChange::~SyncChange() = default;

std::string SyncChange::ToString() const {
  return "{ " + location_.ToString() +
         ", changeType: " + ChangeTypeToString(change_type_) +
         ", syncData: " + sync_data_.ToString() + "}";
}

void PrintTo(const SyncChange& sync_change, std::ostream* os) {
  *os << sync_change.ToString();
}

}  // namespace syncer
