// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_COMMIT_QUEUE_H_
#define COMPONENTS_SYNC_TEST_MOCK_COMMIT_QUEUE_H_

#include "components/sync/engine/commit_queue.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockCommitQueue : public CommitQueue {
 public:
  MockCommitQueue();
  ~MockCommitQueue() override;

  MOCK_METHOD(void, NudgeForCommit, (), (override));
};

}  //  namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_COMMIT_QUEUE_H_
