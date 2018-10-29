// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/sync_scheduler.h"

#include "chromeos/components/proximity_auth/logging/logging.h"

namespace cryptauth {

SyncScheduler::SyncRequest::SyncRequest(
    base::WeakPtr<SyncScheduler> sync_scheduler)
    : sync_scheduler_(sync_scheduler), completed_(false) {
}

SyncScheduler::SyncRequest::~SyncRequest() {
  if (!completed_)
    PA_LOG(ERROR) << "SyncRequest destroyed without ever having completed";
}

void SyncScheduler::SyncRequest::OnDidComplete(bool success) {
  if (sync_scheduler_) {
    sync_scheduler_->OnSyncCompleted(success);
    sync_scheduler_.reset();
    completed_ = true;
  } else {
    PA_LOG(ERROR) << "SyncRequest completed, but SyncScheduler destroyed.";
  }
}

void SyncScheduler::SyncRequest::Cancel() {
  DCHECK(!completed_);
  completed_ = true;
}

}  // namespace cryptauth
