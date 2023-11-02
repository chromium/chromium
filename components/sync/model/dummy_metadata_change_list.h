// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DUMMY_METADATA_CHANGE_LIST_H_
#define COMPONENTS_SYNC_MODEL_DUMMY_METADATA_CHANGE_LIST_H_

#include <string>

#include "components/sync/model/metadata_change_list.h"

namespace syncer {

// A MetadataChangeList class that does not store anything.
class DummyMetadataChangeList : public MetadataChangeList {
 public:
  DummyMetadataChangeList();
  ~DummyMetadataChangeList() override;

  // MetadataChangeList implementation.
  void UpdateModelTypeState(
      const sync_pb::ModelTypeState& model_type_state) override;
  void ClearModelTypeState() override;
  void UpdateMetadata(const std::string& storage_key,
                      const sync_pb::EntityMetadata& metadata) override;
  void ClearMetadata(const std::string& storage_key) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DUMMY_METADATA_CHANGE_LIST_H_
