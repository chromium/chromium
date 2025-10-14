// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_MIGRATION_TAB_GROUP_ENTITY_CONVERTER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_MIGRATION_TAB_GROUP_ENTITY_CONVERTER_H_

#include <memory>

#include "components/data_sharing/migration/public/migratable_entity_converter.h"

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace tab_groups {

// A utility class responsible for converting Tab Group entities between their
// private (SavedTabGroupSpecifics) and shared (SharedTabGroupData) formats.
class TabGroupEntityConverter : public data_sharing::MigratableEntityConverter {
 public:
  TabGroupEntityConverter() = default;
  ~TabGroupEntityConverter() override = default;

  // Disallow copy/assign.
  TabGroupEntityConverter(const TabGroupEntityConverter&) = delete;
  TabGroupEntityConverter& operator=(const TabGroupEntityConverter&) = delete;

  // data_sharing::MigratableEntityConverter implementation.
  std::unique_ptr<syncer::EntityData> CreateSharedEntityFromPrivate(
      const syncer::EntityData& private_entity) override;

  std::unique_ptr<syncer::EntityData> CreatePrivateEntityFromShared(
      const syncer::EntityData& shared_entity) override;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_MIGRATION_TAB_GROUP_ENTITY_CONVERTER_H_
