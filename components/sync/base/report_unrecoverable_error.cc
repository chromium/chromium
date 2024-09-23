// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/report_unrecoverable_error.h"

#include "base/debug/dump_without_crashing.h"
#include "base/rand_util.h"

namespace syncer {

void ReportUnrecoverableError(version_info::Channel channel) {
  // Only upload on canary/dev builds to avoid overwhelming crash server.
  if (channel != version_info::Channel::CANARY &&
      channel != version_info::Channel::DEV) {
    return;
  }

  // We only want to upload |kErrorUploadRatio| ratio of errors.
  // Note: crash reporting is disabled, and should only be enabled when
  // investigating a specific datatype error. In that event, a specific bug
  // should be referenced here.
  const double kErrorUploadRatio = 0.00;
  if (kErrorUploadRatio <= 0.0) {
    return;  // We are not allowed to upload errors.
  }
  double random_number = base::RandDouble();
  if (random_number > kErrorUploadRatio) {
    return;
  }

  base::debug::DumpWithoutCrashing();
}

}  // namespace syncer
