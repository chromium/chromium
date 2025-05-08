// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/collaboration_group_util.h"

#include "components/sync/base/time.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/entity_builder_factory.h"
#include "components/sync/test/fake_server.h"

namespace collaboration_group_utils {

sync_pb::CollaborationGroupSpecifics MakeCollaborationGroupSpecifics(
    const std::string& group_id) {
  sync_pb::CollaborationGroupSpecifics result;
  result.set_collaboration_id(group_id);

  base::Time now = base::Time::Now();
  result.set_changed_at_timestamp_millis_since_unix_epoch(
      now.InMillisecondsSinceUnixEpoch());
  result.set_consistency_token(
      base::NumberToString(now.InMillisecondsSinceUnixEpoch()));
  return result;
}

syncer::EntityData EntityDataFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_collaboration_group() = specifics;
  entity_data.name = specifics.collaboration_id();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> EntityChangeAddFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateAdd(specifics.collaboration_id(),
                                         EntityDataFromSpecifics(specifics));
}

std::unique_ptr<syncer::EntityChange> EntityChangeUpdateFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateUpdate(specifics.collaboration_id(),
                                            EntityDataFromSpecifics(specifics));
}

std::unique_ptr<syncer::EntityChange> EntityChangeDeleteFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateDelete(specifics.collaboration_id(),
                                            syncer::EntityData());
}

}  // namespace collaboration_group_utils
