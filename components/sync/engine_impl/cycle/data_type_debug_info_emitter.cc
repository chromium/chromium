// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/cycle/data_type_debug_info_emitter.h"

#include <string>

#include "base/metrics/histogram.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"

namespace syncer {

namespace {

const char kModelTypeEntityChangeHistogramPrefix[] =
    "Sync.ModelTypeEntityChange2.";

// Values corrospond to a UMA histogram, do not modify, or delete any values.
// Add new values only directly before COUNT.
enum ModelTypeEntityChange {
  LOCAL_DELETION = 0,
  LOCAL_CREATION = 1,
  LOCAL_UPDATE = 2,
  REMOTE_DELETION = 3,
  REMOTE_NON_INITIAL_UPDATE = 4,
  REMOTE_INITIAL_UPDATE = 5,
  MODEL_TYPE_ENTITY_CHANGE_COUNT = 6
};

void EmitNewChangesToUma(int count,
                         ModelTypeEntityChange bucket,
                         base::HistogramBase* histogram) {
  DCHECK_GE(count, 0);
  if (count > 0) {
    histogram->AddCount(bucket, count);
  }
}

// We'll add many values in this histogram. Since the name of the histogram is
// not static here, we cannot use the (efficient) macros that use caching of the
// histogram object. The helper functions like UmaHistogramEnumeration are not
// efficient and thus we need re-implement the code here, caching the resulting
// histogram object.
base::HistogramBase* GetModelTypeEntityChangeHistogram(ModelType type) {
  std::string type_string = ModelTypeToHistogramSuffix(type);
  std::string full_histogram_name =
      kModelTypeEntityChangeHistogramPrefix + type_string;
  return base::LinearHistogram::FactoryGet(
      /*name=*/full_histogram_name, /*minimum=*/1,
      /*maximum=*/MODEL_TYPE_ENTITY_CHANGE_COUNT,
      /*bucket_count=*/MODEL_TYPE_ENTITY_CHANGE_COUNT + 1,
      /*flagz=*/base::HistogramBase::kUmaTargetedHistogramFlag);
}

}  // namespace

DataTypeDebugInfoEmitter::DataTypeDebugInfoEmitter(ModelType type,
                                                   ObserverListType* observers)
    : type_(type),
      type_debug_info_observers_(observers),
      histogram_(GetModelTypeEntityChangeHistogram(type)) {
  DCHECK(histogram_);
}

DataTypeDebugInfoEmitter::~DataTypeDebugInfoEmitter() {}

const CommitCounters& DataTypeDebugInfoEmitter::GetCommitCounters() const {
  return commit_counters_;
}

CommitCounters* DataTypeDebugInfoEmitter::GetMutableCommitCounters() {
  return &commit_counters_;
}

void DataTypeDebugInfoEmitter::EmitCommitCountersUpdate() {
  for (auto& observer : *type_debug_info_observers_)
    observer.OnCommitCountersUpdated(type_, commit_counters_);

  // Emit the newly added counts to UMA.
  EmitNewChangesToUma(
      /*count=*/commit_counters_.num_creation_commits_attempted -
          emitted_commit_counters_.num_creation_commits_attempted,
      /*bucket=*/LOCAL_CREATION, histogram_);
  EmitNewChangesToUma(
      /*count=*/commit_counters_.num_deletion_commits_attempted -
          emitted_commit_counters_.num_deletion_commits_attempted,
      /*bucket=*/LOCAL_DELETION, histogram_);
  EmitNewChangesToUma(
      /*count=*/commit_counters_.num_update_commits_attempted -
          emitted_commit_counters_.num_update_commits_attempted,
      /*bucket=*/LOCAL_UPDATE, histogram_);

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
  for (auto& observer : *type_debug_info_observers_)
    observer.OnUpdateCountersUpdated(type_, update_counters_);

  // Emit the newly added counts to UMA.
  EmitNewChangesToUma(
      /*count=*/update_counters_.num_initial_updates_received -
          emitted_update_counters_.num_initial_updates_received,
      /*bucket=*/REMOTE_INITIAL_UPDATE, histogram_);
  EmitNewChangesToUma(
      /*count=*/update_counters_.num_non_initial_tombstone_updates_received -
          emitted_update_counters_.num_non_initial_tombstone_updates_received,
      /*bucket=*/REMOTE_DELETION, histogram_);
  // The remote_non_initial_update type is not explicitly stored, we need to
  // compute it as a diff of (all - deletions).
  int emitted_remote_non_initial_updates_count =
      emitted_update_counters_.num_non_initial_updates_received -
      emitted_update_counters_.num_non_initial_tombstone_updates_received;
  int remote_non_initial_updates_count =
      update_counters_.num_non_initial_updates_received -
      update_counters_.num_non_initial_tombstone_updates_received;
  EmitNewChangesToUma(
      /*count=*/remote_non_initial_updates_count -
          emitted_remote_non_initial_updates_count,
      /*bucket=*/REMOTE_NON_INITIAL_UPDATE, histogram_);

  // Mark the current state of the counters as uploaded to UMA.
  emitted_update_counters_ = update_counters_;
}

void DataTypeDebugInfoEmitter::EmitStatusCountersUpdate() {}

}  // namespace syncer
