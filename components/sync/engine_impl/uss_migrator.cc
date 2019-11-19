// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/uss_migrator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/sync/base/time.h"
#include "components/sync/engine_impl/model_type_worker.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/user_share.h"

namespace syncer {

namespace {

bool ExtractSyncEntity(ModelType type,
                       ReadTransaction* trans,
                       int64_t id,
                       sync_pb::SyncEntity* entity) {
  ReadNode read_node(trans);
  if (read_node.InitByIdLookup(id) != BaseNode::INIT_OK)
    return false;

  const syncable::Entry& entry = *read_node.GetEntry();

  // Copy the fields USS cares about from the server side of the directory so
  // that we don't miss things that haven't been applied yet. See
  // ModelTypeWorker::ProcessGetUpdatesResponse for which fields are used.
  entity->set_id_string(entry.GetId().GetServerId());
  entity->set_version(entry.GetServerVersion());
  entity->set_mtime(TimeToProtoTime(entry.GetServerMtime()));
  entity->set_ctime(TimeToProtoTime(entry.GetServerCtime()));
  entity->set_name(entry.GetServerNonUniqueName());
  entity->set_deleted(entry.GetServerIsDel());
  entity->set_client_defined_unique_tag(entry.GetUniqueClientTag());
  // Required fields for bookmarks only.
  entity->set_folder(entry.GetServerIsDir());
  if (!entry.GetServerParentId().IsNull()) {
    entity->set_parent_id_string(entry.GetServerParentId().GetServerId());
  }

  if (!entry.GetUniqueServerTag().empty()) {
    // Permanent nodes don't have unique_positions and are assigned unique
    // server tags.
    entity->set_server_defined_unique_tag(entry.GetUniqueServerTag());
  } else if (entry.GetServerUniquePosition().IsValid()) {
    *entity->mutable_unique_position() =
        entry.GetServerUniquePosition().ToProto();
  } else {
    // All boookmarks (except permanent ones with server tag) should have valid
    // unique_positions including legacy bookmarks that are missing the field.
    // Directory should have taken care of assigning proper unique_position
    // during the first sync flow.
    DCHECK_NE(BOOKMARKS, type);
  }

  entity->mutable_specifics()->CopyFrom(entry.GetServerSpecifics());
  return true;
}

void AppendAllDescendantIds(const ReadTransaction* trans,
                            const ReadNode& node,
                            std::vector<int64_t>* all_descendant_ids) {
  std::vector<int64_t> child_ids;
  node.GetChildIds(&child_ids);

  for (int child_id : child_ids) {
    all_descendant_ids->push_back(child_id);
    ReadNode child(trans);
    child.InitByIdLookup(child_id);
    AppendAllDescendantIds(trans, child, all_descendant_ids);
  }
}

bool MigrateDirectoryDataWithBatchSize(ModelType type,
                                       int batch_size,
                                       UserShare* user_share,
                                       ModelTypeWorker* worker,
                                       int* cumulative_migrated_entity_count) {
  ReadTransaction trans(FROM_HERE, user_share);

  ReadNode root(&trans);
  if (root.InitTypeRoot(type) != BaseNode::INIT_OK) {
    LOG(ERROR) << "Missing root node for " << ModelTypeToString(type);
    // Inform the worker so it can trigger a fallback initial GetUpdates.
    worker->AbortMigration();
    return false;
  }

  // Get the progress marker and context from the directory.
  sync_pb::DataTypeProgressMarker progress;
  sync_pb::DataTypeContext context;
  user_share->directory->GetDownloadProgress(type, &progress);
  user_share->directory->GetDataTypeContext(trans.GetWrappedTrans(), type,
                                            &context);

  std::vector<int64_t> child_ids;
  AppendAllDescendantIds(&trans, root, &child_ids);

  // Process |batch_size| entities at a time to reduce memory usage.
  size_t i = 0;

  // We use |do {} while| to guarantee that, even if there are no entities to
  // process, we call ProcessGetUpdatesResponse() at least once in order to feed
  // the progress marker.
  do {
    // Vector to own the temporary entities.
    std::vector<std::unique_ptr<sync_pb::SyncEntity>> entities;
    // Vector of raw pointers for passing to ProcessGetUpdatesResponse().
    SyncEntityList entity_ptrs;

    const size_t batch_limit = std::min(i + batch_size, child_ids.size());
    for (; i < batch_limit; i++) {
      auto entity = std::make_unique<sync_pb::SyncEntity>();
      if (!ExtractSyncEntity(type, &trans, child_ids[i], entity.get())) {
        LOG(ERROR) << "Failed to fetch child node for "
                   << ModelTypeToString(type);
        // Inform the worker so it can clear any partial data and trigger a
        // fallback initial GetUpdates.
        worker->AbortMigration();
        return false;
      }
      // Ignore tombstones; they are not included for initial GetUpdates.
      if (!entity->deleted()) {
        entity_ptrs.push_back(entity.get());
        entities.push_back(std::move(entity));
      }
    }

    *cumulative_migrated_entity_count += entity_ptrs.size();

    worker->ProcessGetUpdatesResponse(progress, context, entity_ptrs,
                                      /*from_uss_migrator=*/true,
                                      /*status=*/nullptr);
  } while (i != child_ids.size());

  worker->PassiveApplyUpdates(nullptr);
  return true;
}

}  // namespace

bool MigrateDirectoryData(ModelType type,
                          UserShare* user_share,
                          ModelTypeWorker* worker,
                          int* migrated_entity_count) {
  *migrated_entity_count = 0;
  return MigrateDirectoryDataWithBatchSize(type, 64, user_share, worker,
                                           migrated_entity_count);
}

bool MigrateDirectoryDataWithBatchSizeForTesting(
    ModelType type,
    int batch_size,
    UserShare* user_share,
    ModelTypeWorker* worker,
    int* cumulative_migrated_entity_count) {
  return MigrateDirectoryDataWithBatchSize(type, batch_size, user_share, worker,
                                           cumulative_migrated_entity_count);
}

}  // namespace syncer
