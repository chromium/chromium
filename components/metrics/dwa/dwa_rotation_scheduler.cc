// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_rotation_scheduler.h"

#include "base/metrics/histogram_macros.h"

namespace metrics::dwa {

DwaRotationScheduler::DwaRotationScheduler(
    const base::RepeatingClosure& upload_callback,
    const base::RepeatingCallback<base::TimeDelta(void)>&
        upload_interval_callback,
    bool fast_startup)
    : metrics::MetricsRotationScheduler(upload_callback,
                                        upload_interval_callback,
                                        fast_startup) {}

DwaRotationScheduler::~DwaRotationScheduler() = default;

void DwaRotationScheduler::LogMetricsInitSequence(InitSequence sequence) {
  UMA_HISTOGRAM_ENUMERATION("DWA.InitSequence", sequence,
                            INIT_SEQUENCE_ENUM_SIZE);
}

}  // namespace metrics::dwa
