// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_NUDGE_HANDLER_H_
#define COMPONENTS_SYNC_TEST_MOCK_NUDGE_HANDLER_H_

#include "base/compiler_specific.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/nudge_handler.h"

namespace syncer {

class MockNudgeHandler : public NudgeHandler {
 public:
  MockNudgeHandler();

  MockNudgeHandler(const MockNudgeHandler&) = delete;
  MockNudgeHandler& operator=(const MockNudgeHandler&) = delete;

  ~MockNudgeHandler() override;

  void NudgeForInitialDownload(ModelType type) override;
  void NudgeForCommit(ModelType type) override;
  void SetHasPendingInvalidations(ModelType type,
                                  bool has_pending_invalidations) override;

  int GetNumInitialDownloadNudges() const;
  int GetNumCommitNudges() const;

  void ClearCounters();

 private:
  int num_initial_nudges_;
  int num_commit_nudges_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_NUDGE_HANDLER_H_
