// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_NUDGE_HANDLER_H_
#define COMPONENTS_SYNC_TEST_MOCK_NUDGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/nudge_handler.h"

namespace syncer {

class MockNudgeHandler : public NudgeHandler {
 public:
  MockNudgeHandler();

  MockNudgeHandler(const MockNudgeHandler&) = delete;
  MockNudgeHandler& operator=(const MockNudgeHandler&) = delete;

  ~MockNudgeHandler() override;

  void NudgeForInitialDownload(DataType type) override;
  void NudgeForCommit(DataType type) override;
  void SetHasPendingInvalidations(DataType type,
                                  bool has_pending_invalidations) override;

  int GetNumInitialDownloadNudges() const;
  int GetNumCommitNudges() const;

  void ClearCounters();

 private:
  int num_initial_nudges_ = 0;
  int num_commit_nudges_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_NUDGE_HANDLER_H_
