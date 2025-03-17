// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/entity_change.h"

#include <utility>

#include "base/memory/ptr_util.h"

namespace syncer {

// static
std::unique_ptr<EntityChange> EntityChange::CreateAdd(
    const std::string& storage_key,
    EntityData data) {
  return base::WrapUnique(
      new EntityChange(storage_key, ACTION_ADD, std::move(data)));
}

// static
std::unique_ptr<EntityChange> EntityChange::CreateUpdate(
    const std::string& storage_key,
    EntityData data) {
  return base::WrapUnique(
      new EntityChange(storage_key, ACTION_UPDATE, std::move(data)));
}

// static
std::unique_ptr<EntityChange> EntityChange::CreateDelete(
    const std::string& storage_key,
    EntityData data) {
  return base::WrapUnique(
      new EntityChange(storage_key, ACTION_DELETE, std::move(data)));
}

// static
std::unique_ptr<EntityChange>
EntityChange::CreateDeletedCollaborationMembership(
    const std::string& storage_key) {
  // EntityData() is not used for the deleted collaboration membership because
  // this is not a remote entity update.
  std::unique_ptr<EntityChange> entity_change =
      CreateDelete(storage_key, EntityData());
  entity_change->is_deleted_collaboration_membership_ = true;
  return entity_change;
}

EntityChange::EntityChange(const std::string& storage_key,
                           ChangeType type,
                           EntityData data)
    : storage_key_(storage_key), type_(type), data_(std::move(data)) {}

EntityChange::~EntityChange() = default;

}  // namespace syncer
