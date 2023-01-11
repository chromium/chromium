// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_USER_EVENTS_GLOBAL_ID_MAPPER_H_
#define COMPONENTS_SYNC_USER_EVENTS_GLOBAL_ID_MAPPER_H_

#include <stdint.h>

#include "base/functional/callback.h"

namespace syncer {

using GlobalIdChange =
    base::RepeatingCallback<void(int64_t old_global_id, int64_t new_global_id)>;

// UserEventSpecifics references SESSIONS data through their |navigation_id|
// which will match values seen in TabNavigation's |global_id|. This field is
// really just the timestamp from the NavigationEntry, which can change when a
// page is reloaded. This will cause the SESSIONS side to override the old data.
// The impact to USER_EVENTS depends on if the previous timestamp made it to the
// server as part of SESSIONS data or not. If it did not, then USER_EVENTS must
// be updated. But knowing the answer that question is currently very tricky
// with Sync's current architecture, so instead we always update for uncommitted
// USER_EVENTS. The purpose of the GlobalIdMapper is the track changes in the
// SESSIONS' |global_id| field and feed this information the USER_EVENTS side.
class GlobalIdMapper {
 public:
  // Register for information about changing
  virtual void AddGlobalIdChangeObserver(GlobalIdChange callback) = 0;

  // Given a |global_id|, returns what the latest global_id is for the given
  // navigation, to the best of our ability. If we do not have such a mapping,
  // which is quite possible, the input |global_id| is returned.
  virtual int64_t GetLatestGlobalId(int64_t global_id) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_USER_EVENTS_GLOBAL_ID_MAPPER_H_
