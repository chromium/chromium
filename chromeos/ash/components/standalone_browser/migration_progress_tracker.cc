// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/migration_progress_tracker.h"

#include <algorithm>

#include "base/logging.h"

namespace ash::standalone_browser {

MigrationProgressTrackerImpl::MigrationProgressTrackerImpl(
    const ProgressCallback& callback)
    : progress_callback_(callback) {}

MigrationProgressTrackerImpl::~MigrationProgressTrackerImpl() = default;

void MigrationProgressTrackerImpl::UpdateProgress(int64_t size) {
  DCHECK_NE(total_size_to_copy_, -1);

  if (total_size_to_copy_ == 0) {
    LOG(WARNING) << "total_size_to_copy_ is zero.";
    progress_callback_.Run(0);
    return;
  }

  size_copied_ += size;

  int new_progress = static_cast<int>(size_copied_ * 100 / total_size_to_copy_);
  new_progress = std::clamp(new_progress, 0, 100);

  if (progress_ < new_progress) {
    progress_ = new_progress;
    progress_callback_.Run(new_progress);
  }
}

void MigrationProgressTrackerImpl::SetTotalSizeToCopy(int64_t size) {
  // Ensure that the method has not been called.
  DCHECK_EQ(total_size_to_copy_, -1);

  total_size_to_copy_ = size;
}

}  // namespace ash::standalone_browser
