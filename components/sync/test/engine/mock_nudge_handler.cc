// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/mock_nudge_handler.h"

namespace syncer {

MockNudgeHandler::MockNudgeHandler()
    : num_initial_nudges_(0), num_commit_nudges_(0) {}

MockNudgeHandler::~MockNudgeHandler() {}

void MockNudgeHandler::NudgeForInitialDownload(ModelType type) {
  num_initial_nudges_++;
}

void MockNudgeHandler::NudgeForCommit(ModelType type) {
  num_commit_nudges_++;
}

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
