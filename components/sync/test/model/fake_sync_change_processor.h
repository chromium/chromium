// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MODEL_FAKE_SYNC_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_TEST_MODEL_FAKE_SYNC_CHANGE_PROCESSOR_H_

#include <string>

#include "base/macros.h"
#include "components/sync/model/sync_change_processor.h"

namespace syncer {

class FakeSyncChangeProcessor : public SyncChangeProcessor {
 public:
  FakeSyncChangeProcessor();
  ~FakeSyncChangeProcessor() override;

  // SyncChangeProcessor implementation.
  //
  // ProcessSyncChanges will accumulate changes in changes() until they are
  // cleared.
  base::Optional<ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const SyncChangeList& change_list) override;

  // SyncChangeProcessor implementation.
  //
  // Returns data().
  SyncDataList GetAllSyncData(ModelType type) const override;

  virtual const SyncChangeList& changes() const;
  virtual SyncChangeList& changes();

  virtual const SyncDataList& data() const;
  virtual SyncDataList& data();

 private:
  SyncChangeList changes_;
  SyncDataList data_;

  DISALLOW_COPY_AND_ASSIGN(FakeSyncChangeProcessor);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MODEL_FAKE_SYNC_CHANGE_PROCESSOR_H_
