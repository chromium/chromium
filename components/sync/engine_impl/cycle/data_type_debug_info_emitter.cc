// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/data_type_debug_info_emitter.h"

#include <string>

#include "base/metrics/histogram.h"

namespace syncer {

DataTypeDebugInfoEmitter::DataTypeDebugInfoEmitter(ModelType type) {}

DataTypeDebugInfoEmitter::~DataTypeDebugInfoEmitter() {}

const CommitCounters& DataTypeDebugInfoEmitter::GetCommitCounters() const {
  return commit_counters_;
}

CommitCounters* DataTypeDebugInfoEmitter::GetMutableCommitCounters() {
  return &commit_counters_;
}

void DataTypeDebugInfoEmitter::EmitCommitCountersUpdate() {
  // Mark the current state of the counters as uploaded to UMA.
  emitted_commit_counters_ = commit_counters_;
}

const UpdateCounters& DataTypeDebugInfoEmitter::GetUpdateCounters() const {
  return update_counters_;
}

UpdateCounters* DataTypeDebugInfoEmitter::GetMutableUpdateCounters() {
  return &update_counters_;
}

void DataTypeDebugInfoEmitter::EmitUpdateCountersUpdate() {
  // Mark the current state of the counters as uploaded to UMA.
  emitted_update_counters_ = update_counters_;
}

}  // namespace syncer
