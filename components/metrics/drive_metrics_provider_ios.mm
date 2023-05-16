// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/drive_metrics_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace metrics {

// static
bool DriveMetricsProvider::HasSeekPenalty(const base::FilePath& path,
                                          bool* has_seek_penalty) {
  *has_seek_penalty = false;
  return true;
}

}  // namespace metrics
