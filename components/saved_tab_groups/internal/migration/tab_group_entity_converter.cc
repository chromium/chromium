// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/migration/tab_group_entity_converter.h"

#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"

namespace tab_groups {

namespace {

// Converts the core data from a private SavedTabGroup proto to a shared
// SharedTabGroup proto.
void ConvertGroupPrivateToShared(const sync_pb::SavedTabGroup& private_group,
                                 const std::string& group_guid,
                                 sync_pb::SharedTabGroup* shared_group) {
  shared_group->set_title(private_group.title());
  shared_group->set_color(
      static_cast<sync_pb::SharedTabGroup::Color>(private_group.color()));
  shared_group->set_originating_tab_group_guid(group_guid);
}

// Converts the core data from a private SavedTab proto to a shared
// SharedTab proto.
void ConvertTabPrivateToShared(const sync_pb::SavedTabGroupTab& private_tab,
                               sync_pb::SharedTab* shared_tab) {
  shared_tab->set_url(private_tab.url());
  shared_tab->set_title(private_tab.title());
  shared_tab->set_shared_tab_group_guid(private_tab.group_guid());
}

// Converts the core data from a shared SharedTabGroup proto back to a private
// SavedTabGroup proto.
void ConvertGroupSharedToPrivate(const sync_pb::SharedTabGroup& shared_group,
                                 sync_pb::SavedTabGroup* private_group) {
  private_group->set_title(shared_group.title());
  private_group->set_color(
      static_cast<sync_pb::SavedTabGroup::SavedTabGroupColor>(
          shared_group.color()));
}

// Converts the core data from a shared SharedTab proto back to a
// private SavedTab proto.
void ConvertTabSharedToPrivate(const sync_pb::SharedTab& shared_tab,
                               sync_pb::SavedTabGroupTab* private_tab) {
  private_tab->set_url(shared_tab.url());
  private_tab->set_title(shared_tab.title());
  private_tab->set_group_guid(shared_tab.shared_tab_group_guid());
}

}  // namespace

std::unique_ptr<syncer::EntityData>
TabGroupEntityConverter::CreateSharedEntityFromPrivate(
    const syncer::EntityData& private_entity) {
  auto shared_entity = std::make_unique<syncer::EntityData>();
  shared_entity->name = private_entity.name;
  shared_entity->client_tag_hash = private_entity.client_tag_hash;

  const sync_pb::SavedTabGroupSpecifics& private_specifics =
      private_entity.specifics.saved_tab_group();
  sync_pb::SharedTabGroupDataSpecifics& shared_specifics =
      *shared_entity->specifics.mutable_shared_tab_group_data();

  shared_specifics.set_guid(private_specifics.guid());
  shared_specifics.set_update_time_windows_epoch_micros(
      private_specifics.update_time_windows_epoch_micros());

  if (private_specifics.has_group()) {
    ConvertGroupPrivateToShared(private_specifics.group(),
                                private_specifics.guid(),
                                shared_specifics.mutable_tab_group());
  } else if (private_specifics.has_tab()) {
    ConvertTabPrivateToShared(private_specifics.tab(),
                              shared_specifics.mutable_tab());
  }

  return shared_entity;
}

std::unique_ptr<syncer::EntityData>
TabGroupEntityConverter::CreatePrivateEntityFromShared(
    const syncer::EntityData& shared_entity) {
  auto private_entity = std::make_unique<syncer::EntityData>();
  private_entity->name = shared_entity.name;
  private_entity->client_tag_hash = shared_entity.client_tag_hash;

  const sync_pb::SharedTabGroupDataSpecifics& shared_specifics =
      shared_entity.specifics.shared_tab_group_data();
  sync_pb::SavedTabGroupSpecifics& private_specifics =
      *private_entity->specifics.mutable_saved_tab_group();

  private_specifics.set_guid(shared_specifics.guid());
  private_specifics.set_creation_time_windows_epoch_micros(
      shared_specifics.update_time_windows_epoch_micros());
  private_specifics.set_update_time_windows_epoch_micros(
      shared_specifics.update_time_windows_epoch_micros());

  if (shared_specifics.has_tab_group()) {
    ConvertGroupSharedToPrivate(shared_specifics.tab_group(),
                                private_specifics.mutable_group());
  } else if (shared_specifics.has_tab()) {
    ConvertTabSharedToPrivate(shared_specifics.tab(),
                              private_specifics.mutable_tab());
  }

  return private_entity;
}

}  // namespace tab_groups
