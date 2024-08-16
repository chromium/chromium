// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_nudge_handler.h"

namespace syncer {

MockNudgeHandler::MockNudgeHandler() = default;

MockNudgeHandler::~MockNudgeHandler() = default;

void MockNudgeHandler::NudgeForInitialDownload(DataType type) {
  num_initial_nudges_++;
}

void MockNudgeHandler::NudgeForCommit(DataType type) {
  num_commit_nudges_++;
}

void MockNudgeHandler::SetHasPendingInvalidations(
    DataType type,
    bool has_pending_invalidations) {}

int MockNudgeHandler::GetNumInitialDownloadNudges() const {
  return num_initial_nudges_;
}

int MockNudgeHandler::GetNumCommitNudges() const {
  return num_commit_nudges_;
}

void MockNudgeHandler::ClearCounters() {
  num_initial_nudges_ = 0;
  num_commit_nudges_ = 0;
}

}  // namespace syncer
