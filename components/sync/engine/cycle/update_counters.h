// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CYCLE_UPDATE_COUNTERS_H_
#define COMPONENTS_SYNC_ENGINE_CYCLE_UPDATE_COUNTERS_H_

#include <memory>
#include <string>

#include "base/values.h"

namespace syncer {

// A class to maintain counts related to the update requests and responses for
// a particular sync type.
struct UpdateCounters {
  UpdateCounters();
  ~UpdateCounters();

  std::unique_ptr<base::DictionaryValue> ToValue() const;
  std::string ToString() const;

  int num_initial_updates_received;
  int num_non_initial_updates_received;
  int num_non_initial_reflected_updates_received;
  int num_non_initial_tombstone_updates_received;

  int num_updates_applied;
  int num_hierarchy_conflict_application_failures;
  int num_encryption_conflict_application_failures;

  int num_server_overwrites;
  int num_local_overwrites;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CYCLE_UPDATE_COUNTERS_H_
