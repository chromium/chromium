// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/sync_change_processor_wrapper_for_test.h"

#include "base/bind.h"
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

SyncChangeProcessorWrapperForTest::~SyncChangeProcessorWrapperForTest() {}

SyncError SyncChangeProcessorWrapperForTest::ProcessSyncChanges(
    const base::Location& from_here,
    const SyncChangeList& change_list) {
  return process_sync_changes_.Run(from_here, change_list);
}

SyncDataList SyncChangeProcessorWrapperForTest::GetAllSyncData(
    ModelType type) const {
  NOTREACHED();
  return SyncDataList();
}

}  // namespace syncer
