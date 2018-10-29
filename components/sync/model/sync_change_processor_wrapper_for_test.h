// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_WRAPPER_FOR_TEST_H_
#define COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_WRAPPER_FOR_TEST_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/sync/model/sync_change_processor.h"

namespace syncer {

class SyncableService;

// A wrapper class for use in tests that forwards changes to a SyncableService
// or a SyncChangeProcessor;
class SyncChangeProcessorWrapperForTest : public SyncChangeProcessor {
 public:
  // Create a SyncChangeProcessorWrapperForTest.
  //
  // All method calls are forwarded to |wrapped|. Caller maintains ownership
  // of |wrapped| and is responsible for ensuring it outlives this object.
  explicit SyncChangeProcessorWrapperForTest(SyncChangeProcessor* wrapped);
  // Overload for SyncableService.
  explicit SyncChangeProcessorWrapperForTest(SyncableService* wrapped);
  ~SyncChangeProcessorWrapperForTest() override;

  // SyncChangeProcessor implementation.
  SyncError ProcessSyncChanges(const base::Location& from_here,
                               const SyncChangeList& change_list) override;
  SyncDataList GetAllSyncData(ModelType type) const override;

 private:
  const base::RepeatingCallback<SyncError(const base::Location& from_here,
                                          const SyncChangeList& change_list)>
      process_sync_changes_;

  DISALLOW_COPY_AND_ASSIGN(SyncChangeProcessorWrapperForTest);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_CHANGE_PROCESSOR_WRAPPER_FOR_TEST_H_
