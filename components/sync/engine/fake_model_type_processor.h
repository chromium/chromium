// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_FAKE_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_ENGINE_FAKE_MODEL_TYPE_PROCESSOR_H_

#include <memory>

#include "components/sync/engine/model_type_processor.h"

namespace syncer {

class FakeModelTypeProcessor : public ModelTypeProcessor {
 public:
  FakeModelTypeProcessor();
  ~FakeModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(const sync_pb::ModelTypeState& type_state,
                         const CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        UpdateResponseDataList updates) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_FAKE_MODEL_TYPE_PROCESSOR_H_
