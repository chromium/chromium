// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_ENTITY_CONVERTER_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_ENTITY_CONVERTER_H_

#include <memory>

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace data_sharing {

// An interface for converting a sync entity between its private format and a
// shared format. This is used during the migration process when a user opts
// to share a previously private data type.
class MigratableEntityConverter {
 public:
  virtual ~MigratableEntityConverter() = default;

  // Creates a new entity in the shared format from a private one.
  virtual std::unique_ptr<syncer::EntityData> CreateSharedEntityFromPrivate(
      const syncer::EntityData& private_entity) = 0;

  // Creates a new entity in the private format from a shared one.
  virtual std::unique_ptr<syncer::EntityData> CreatePrivateEntityFromShared(
      const syncer::EntityData& shared_entity) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_ENTITY_CONVERTER_H_
