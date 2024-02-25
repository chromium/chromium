// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_CHANGE_PROCESSOR_H_

#include "components/sync/model/sync_change_processor.h"

namespace syncer {

class FakeSyncChangeProcessor : public SyncChangeProcessor {
 public:
  FakeSyncChangeProcessor();

  FakeSyncChangeProcessor(const FakeSyncChangeProcessor&) = delete;
  FakeSyncChangeProcessor& operator=(const FakeSyncChangeProcessor&) = delete;

  ~FakeSyncChangeProcessor() override;

  // SyncChangeProcessor implementation.
  //
  // ProcessSyncChanges will accumulate changes in changes() until they are
  // cleared.
  std::optional<ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const SyncChangeList& change_list) override;

  const SyncChangeList& changes() const;
  SyncChangeList& changes();

 private:
  SyncChangeList changes_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_CHANGE_PROCESSOR_H_
