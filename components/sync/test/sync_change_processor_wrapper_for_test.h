// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_SYNC_CHANGE_PROCESSOR_WRAPPER_FOR_TEST_H_
#define COMPONENTS_SYNC_TEST_SYNC_CHANGE_PROCESSOR_WRAPPER_FOR_TEST_H_

#include "base/functional/callback.h"
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

  SyncChangeProcessorWrapperForTest(const SyncChangeProcessorWrapperForTest&) =
      delete;
  SyncChangeProcessorWrapperForTest& operator=(
      const SyncChangeProcessorWrapperForTest&) = delete;

  ~SyncChangeProcessorWrapperForTest() override;

  // SyncChangeProcessor implementation.
  std::optional<ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const SyncChangeList& change_list) override;

 private:
  const base::RepeatingCallback<std::optional<ModelError>(
      const base::Location& from_here,
      const SyncChangeList& change_list)>
      process_sync_changes_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_SYNC_CHANGE_PROCESSOR_WRAPPER_FOR_TEST_H_
