// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model/ephemeral_change.h"

namespace feed {
namespace stream_model {

EphemeralChange::EphemeralChange(
    EphemeralChangeId id,
    std::vector<feedstore::DataOperation> operations)
    : id_(id), operations_(std::move(operations)) {}
EphemeralChange::~EphemeralChange() = default;

EphemeralChangeList::EphemeralChangeList() = default;
EphemeralChangeList::~EphemeralChangeList() = default;
EphemeralChange* EphemeralChangeList::AddEphemeralChange(
    std::vector<feedstore::DataOperation> operations) {
  change_list_.push_back(std::make_unique<EphemeralChange>(
      id_generator_.GenerateNextId(), operations));
  return change_list_.back().get();
}
EphemeralChange* EphemeralChangeList::Find(EphemeralChangeId id) {
  for (std::unique_ptr<EphemeralChange>& change : change_list_) {
    if (change->id() == id)
      return change.get();
  }
  return nullptr;
}

std::unique_ptr<FeatureTree> ApplyEphemeralChanges(
    const FeatureTree& tree,
    const EphemeralChangeList& changes) {
  auto tree_with_changes = std::make_unique<FeatureTree>(&tree);

  for (const std::unique_ptr<EphemeralChange>& change :
       changes.GetChangeList()) {
    for (const feedstore::DataOperation& operation : change->GetOperations()) {
      if (operation.has_structure()) {
        tree_with_changes->ApplyStreamStructure(operation.structure());
      }
      if (operation.has_content()) {
        tree_with_changes->CopyAndAddContent(operation.content());
      }
    }
  }
  return tree_with_changes;
}

std::unique_ptr<EphemeralChange> EphemeralChangeList::Remove(
    EphemeralChangeId id) {
  for (size_t i = 0; i < change_list_.size(); ++i) {
    if (change_list_[i]->id() == id) {
      std::unique_ptr<EphemeralChange> result = std::move(change_list_[i]);
      change_list_.erase(change_list_.begin() + i);
      return result;
    }
  }
  return nullptr;
}

}  // namespace stream_model
}  // namespace feed
