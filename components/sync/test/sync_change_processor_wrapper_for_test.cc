// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/sync_change_processor_wrapper_for_test.h"

#include "base/functional/bind.h"
#include "components/sync/model/syncable_service.h"

namespace syncer {

SyncChangeProcessorWrapperForTest::SyncChangeProcessorWrapperForTest(
    SyncChangeProcessor* wrapped)
    : process_sync_changes_(
          base::BindRepeating(&SyncChangeProcessor::ProcessSyncChanges,
                              base::Unretained(wrapped))) {
  DCHECK(wrapped);
}

SyncChangeProcessorWrapperForTest::SyncChangeProcessorWrapperForTest(
    SyncableService* wrapped)
    : process_sync_changes_(
          base::BindRepeating(&SyncableService::ProcessSyncChanges,
                              base::Unretained(wrapped))) {
  DCHECK(wrapped);
}

SyncChangeProcessorWrapperForTest::~SyncChangeProcessorWrapperForTest() =
    default;

std::optional<ModelError> SyncChangeProcessorWrapperForTest::ProcessSyncChanges(
    const base::Location& from_here,
    const SyncChangeList& change_list) {
  return process_sync_changes_.Run(from_here, change_list);
}

}  // namespace syncer
