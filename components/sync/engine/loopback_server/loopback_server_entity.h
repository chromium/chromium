// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_SERVER_ENTITY_H_
#define COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_SERVER_ENTITY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "components/sync/base/data_type.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace sync_pb {
class SyncEntity;
class LoopbackServerEntity;
enum LoopbackServerEntity_Type : int;
}  // namespace sync_pb

namespace syncer {

// The representation of a Sync entity for the loopback server.
class LoopbackServerEntity {
 public:
  // Creates an ID of the form <type><separator><inner-id> where
  // <type> is the EntitySpecifics field number for |data_type|, <separator>
  // is kIdSeparator, and <inner-id> is |inner_id|.
  //
  // If |inner_id| is globally unique, then the returned ID will also be
  // globally unique.
  static std::string CreateId(const syncer::DataType& data_type,
                              const std::string& inner_id);

  // Returns the ID string of the top level node for the specified type.
  static std::string GetTopLevelId(const syncer::DataType& data_type);

  static std::unique_ptr<LoopbackServerEntity> CreateEntityFromProto(
      const sync_pb::LoopbackServerEntity& entity);

  virtual ~LoopbackServerEntity();
  const std::string& GetId() const;
  syncer::DataType GetDataType() const;
  int64_t GetVersion() const;
  void SetVersion(int64_t version);
  const std::string& GetName() const;
  void SetName(const std::string& name);

  // Replaces |specifics_| with |updated_specifics|. This method is meant to be
  // used to mimic a client commit.
  void SetSpecifics(const sync_pb::EntitySpecifics& updated_specifics);
  sync_pb::EntitySpecifics GetSpecifics() const;

  // Common data items needed by server
  virtual bool RequiresParentId() const = 0;
  virtual std::string GetParentId() const = 0;
  virtual void SerializeAsProto(sync_pb::SyncEntity* proto) const = 0;
  virtual sync_pb::LoopbackServerEntity_Type GetLoopbackServerEntityType()
      const;
  virtual bool IsDeleted() const;
  virtual bool IsFolder() const;
  virtual bool IsPermanent() const;

  virtual void SerializeAsLoopbackServerEntity(
      sync_pb::LoopbackServerEntity* entity) const;

  // Extracts the DataType from |id|. If |id| is malformed or does not contain
  // a valid DataType, UNSPECIFIED is returned.
  static syncer::DataType GetDataTypeFromId(const std::string& id);

  // Extracts the inner ID as specified in the constructor from |id|.
  static std::string GetInnerIdFromId(const std::string& id);

 protected:
  LoopbackServerEntity(const std::string& id,
                       const syncer::DataType& data_type,
                       int64_t version,
                       const std::string& name);

  void SerializeBaseProtoFields(sync_pb::SyncEntity* sync_entity) const;

 private:
  // The entity's ID.
  const std::string id_;

  // The DataType that categorizes this entity.
  const syncer::DataType data_type_;

  // The version of this entity.
  int64_t version_;

  // The name of the entity.
  std::string name_;

  // The EntitySpecifics for the entity.
  sync_pb::EntitySpecifics specifics_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_LOOPBACK_SERVER_LOOPBACK_SERVER_ENTITY_H_
