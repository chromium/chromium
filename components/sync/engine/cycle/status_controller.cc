// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/status_controller.h"

#include "components/sync/base/data_type.h"
#include "components/sync/engine/sync_protocol_error.h"

namespace syncer {

StatusController::StatusController() = default;

StatusController::~StatusController() = default;

DataTypeSet StatusController::get_updated_types() const {
  return model_neutral_.updated_types;
}

void StatusController::add_updated_type(DataType type) {
  model_neutral_.updated_types.Put(type);
}

void StatusController::clear_updated_types() {
  model_neutral_.updated_types.Clear();
}

void StatusController::increment_num_updates_downloaded_by(int value) {
  model_neutral_.num_updates_downloaded_total += value;
}

void StatusController::increment_num_tombstone_updates_downloaded_by(
    int value) {
  model_neutral_.num_tombstone_updates_downloaded_total += value;
}

void StatusController::UpdateStartTime() {
  sync_start_time_ = base::Time::Now();
}

void StatusController::UpdatePollTime() {
  poll_finish_time_ = base::Time::Now();
}

void StatusController::increment_num_successful_bookmark_commits() {
  model_neutral_.num_successful_bookmark_commits++;
}

void StatusController::increment_num_successful_commits() {
  model_neutral_.num_successful_commits++;
}

void StatusController::increment_num_server_conflicts() {
  model_neutral_.num_server_conflicts++;
}

void StatusController::set_last_get_key_failed(bool failed) {
  model_neutral_.last_get_key_failed = failed;
}

void StatusController::set_last_download_updates_result(
    const SyncerError result) {
  model_neutral_.last_download_updates_result = result;
}

void StatusController::set_commit_result(const SyncerError result) {
  model_neutral_.commit_result = result;
}

bool StatusController::last_get_key_failed() const {
  return model_neutral_.last_get_key_failed;
}

int StatusController::num_server_conflicts() const {
  return model_neutral_.num_server_conflicts;
}

int StatusController::TotalNumConflictingItems() const {
  return num_server_conflicts();
}

}  // namespace syncer
