// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_MODEL_EPHEMERAL_CHANGE_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_MODEL_EPHEMERAL_CHANGE_H_

#include <memory>
#include <vector>
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/stream_model/feature_tree.h"
#include "components/feed/core/v2/types.h"

namespace feed {
namespace stream_model {

// A sequence of data operations that may be reverted.
class EphemeralChange {
 public:
  EphemeralChange(EphemeralChangeId id,
                  std::vector<feedstore::DataOperation> operations);
  ~EphemeralChange();
  EphemeralChange(const EphemeralChange&) = delete;
  EphemeralChange& operator=(const EphemeralChange&) = delete;

  EphemeralChangeId id() const { return id_; }
  const std::vector<feedstore::DataOperation>& GetOperations() const {
    return operations_;
  }
  std::vector<feedstore::DataOperation>& GetOperations() { return operations_; }

 private:
  EphemeralChangeId id_;
  std::vector<feedstore::DataOperation> operations_;
};

// A list of |EphemeralChange| objects.
class EphemeralChangeList {
 public:
  EphemeralChangeList();
  ~EphemeralChangeList();
  EphemeralChangeList(const EphemeralChangeList&) = delete;
  EphemeralChangeList& operator=(const EphemeralChangeList&) = delete;

  const std::vector<std::unique_ptr<EphemeralChange>>& GetChangeList() const {
    return change_list_;
  }
  EphemeralChange* Find(EphemeralChangeId id);
  EphemeralChange* AddEphemeralChange(
      std::vector<feedstore::DataOperation> operations);
  std::unique_ptr<EphemeralChange> Remove(EphemeralChangeId id);

 private:
  EphemeralChangeId::Generator id_generator_;
  std::vector<std::unique_ptr<EphemeralChange>> change_list_;
};

// Return a new |FeatureTree| by applying |changes| to |tree|.
std::unique_ptr<FeatureTree> ApplyEphemeralChanges(
    const FeatureTree& tree,
    const EphemeralChangeList& changes);

}  // namespace stream_model
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_STREAM_MODEL_EPHEMERAL_CHANGE_H_
