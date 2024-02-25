// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/metrics_util.h"

#include "base/check.h"

namespace cronet {

namespace metrics_util {

int64_t ConvertTime(const base::TimeTicks& ticks,
                    const base::TimeTicks& start_ticks,
                    const base::Time& start_time) {
  if (ticks.is_null() || start_ticks.is_null()) {
    return kNullTime;
  }
  DCHECK(!start_time.is_null());
  return (start_time + (ticks - start_ticks)).InMillisecondsSinceUnixEpoch();
}

}  // namespace metrics_util

}  // namespace cronet
