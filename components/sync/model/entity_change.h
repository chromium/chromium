// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ENTITY_CHANGE_H_
#define COMPONENTS_SYNC_MODEL_ENTITY_CHANGE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/protocol/entity_data.h"

namespace syncer {

class EntityChange {
 public:
  enum ChangeType { ACTION_ADD, ACTION_UPDATE, ACTION_DELETE };

  static std::unique_ptr<EntityChange> CreateAdd(const std::string& storage_key,
                                                 EntityData data);
  static std::unique_ptr<EntityChange> CreateUpdate(
      const std::string& storage_key,
      EntityData data);
  static std::unique_ptr<EntityChange> CreateDelete(
      const std::string& storage_key);

  EntityChange(const EntityChange&) = delete;
  EntityChange& operator=(const EntityChange&) = delete;

  virtual ~EntityChange();

  std::string storage_key() const { return storage_key_; }
  ChangeType type() const { return type_; }
  const EntityData& data() const { return data_; }

 private:
  EntityChange(const std::string& storage_key,
               ChangeType type,
               EntityData data);

  std::string storage_key_;
  ChangeType type_;
  EntityData data_;
};

using EntityChangeList = std::vector<std::unique_ptr<EntityChange>>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ENTITY_CHANGE_H_
