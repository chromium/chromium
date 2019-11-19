// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_FORWARDING_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_NIGORI_FORWARDING_MODEL_TYPE_PROCESSOR_H_

#include <memory>

#include "base/macros.h"
#include "components/sync/engine/model_type_processor.h"

namespace syncer {

// Trivial implementation of ModelTypeProcessor, that simply forwards
// call to another processor. This is useful when an API requires transferring
// ownership, but the calling site also wants to keep ownership of the actual
// implementation, and can guarantee the lifetime constraints.
class ForwardingModelTypeProcessor : public ModelTypeProcessor {
 public:
  // |processor| must not be null and must outlive this object.
  explicit ForwardingModelTypeProcessor(ModelTypeProcessor* processor);
  ~ForwardingModelTypeProcessor() override;

  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(const sync_pb::ModelTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        UpdateResponseDataList updates) override;

 private:
  ModelTypeProcessor* const processor_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingModelTypeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_FORWARDING_MODEL_TYPE_PROCESSOR_H_
