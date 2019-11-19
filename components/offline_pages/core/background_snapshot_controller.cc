// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background_snapshot_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_page_feature.h"

namespace {
// Default delay, in milliseconds, between the main document OnLoad event and
// snapshot.
const int64_t kDelayAfterDocumentOnLoadCompletedMsBackground = 2000;

// Default delay, in milliseconds, between renovations finishing and
// taking a snapshot. Allows for page to update in response to the
// renovations.
const int64_t kDelayAfterRenovationsCompletedMs = 2000;

// Delay for testing to keep polling times reasonable.
const int64_t kDelayForTests = 0;

}  // namespace

namespace offline_pages {

BackgroundSnapshotController::BackgroundSnapshotController(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    BackgroundSnapshotController::Client* client,
    bool renovations_enabled)
    : task_runner_(task_runner),
      client_(client),
      state_(State::READY),
      delay_after_document_on_load_completed_ms_(
          kDelayAfterDocumentOnLoadCompletedMsBackground),
      delay_after_renovations_completed_ms_(kDelayAfterRenovationsCompletedMs),
      renovations_enabled_(renovations_enabled) {
  if (offline_pages::ShouldUseTestingSnapshotDelay()) {
    delay_after_document_on_load_completed_ms_ = kDelayForTests;
    delay_after_renovations_completed_ms_ = kDelayForTests;
  }
}

BackgroundSnapshotController::~BackgroundSnapshotController() {}

void BackgroundSnapshotController::Reset() {
  // Cancel potentially delayed tasks that relate to the previous 'session'.
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_ = State::READY;
}

void BackgroundSnapshotController::Stop() {
  state_ = State::STOPPED;
}

void BackgroundSnapshotController::RenovationsCompleted() {
  if (renovations_enabled_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BackgroundSnapshotController::MaybeStartSnapshotThenStop,
            weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            delay_after_renovations_completed_ms_));
  }
}

void BackgroundSnapshotController::DocumentOnLoadCompletedInMainFrame() {
  if (renovations_enabled_) {
    // Run renovations. After renovations complete, a snapshot will be
    // triggered after a delay.
    client_->RunRenovations();
  } else {
    // Post a delayed task to snapshot and then stop this controller.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &BackgroundSnapshotController::MaybeStartSnapshotThenStop,
            weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            delay_after_document_on_load_completed_ms_));
  }
}

void BackgroundSnapshotController::MaybeStartSnapshot() {
  if (state_ != State::READY)
    return;
  state_ = State::SNAPSHOT_PENDING;
  client_->StartSnapshot();
}

void BackgroundSnapshotController::MaybeStartSnapshotThenStop() {
  MaybeStartSnapshot();
  Stop();
}

int64_t
BackgroundSnapshotController::GetDelayAfterDocumentOnLoadCompletedForTest() {
  return delay_after_document_on_load_completed_ms_;
}

int64_t
BackgroundSnapshotController::GetDelayAfterRenovationsCompletedForTest() {
  return delay_after_renovations_completed_ms_;
}

}  // namespace offline_pages
