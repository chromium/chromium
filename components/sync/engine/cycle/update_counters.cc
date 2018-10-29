// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/update_counters.h"

#include "base/json/json_string_value_serializer.h"

namespace syncer {

UpdateCounters::UpdateCounters()
    : num_initial_updates_received(0),
      num_non_initial_updates_received(0),
      num_non_initial_reflected_updates_received(0),
      num_non_initial_tombstone_updates_received(0),
      num_updates_applied(0),
      num_hierarchy_conflict_application_failures(0),
      num_encryption_conflict_application_failures(0),
      num_server_overwrites(0),
      num_local_overwrites(0) {}

UpdateCounters::~UpdateCounters() {}

std::unique_ptr<base::DictionaryValue> UpdateCounters::ToValue() const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  value->SetInteger("numInitialUpdatesReceived", num_initial_updates_received);
  value->SetInteger("numUpdatesReceived", num_non_initial_updates_received);
  value->SetInteger("numReflectedUpdatesReceived",
                    num_non_initial_reflected_updates_received);
  value->SetInteger("numTombstoneUpdatesReceived",
                    num_non_initial_tombstone_updates_received);

  value->SetInteger("numUpdatesApplied", num_updates_applied);
  value->SetInteger("numHierarchyConflictApplicationFailures",
                    num_hierarchy_conflict_application_failures);
  value->SetInteger("numEncryptionConflictApplicationFailures",
                    num_encryption_conflict_application_failures);

  value->SetInteger("numServerOverwrites", num_server_overwrites);
  value->SetInteger("numLocalOverwrites", num_local_overwrites);

  return value;
}

std::string UpdateCounters::ToString() const {
  std::string result;
  std::unique_ptr<base::DictionaryValue> value = ToValue();
  JSONStringValueSerializer serializer(&result);
  serializer.Serialize(*value);
  return result;
}

}  // namespace syncer
