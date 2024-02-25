// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/native_metrics_util.h"

#include "components/cronet/metrics_util.h"

namespace cronet {

namespace native_metrics_util {

void ConvertTime(const base::TimeTicks& ticks,
                 const base::TimeTicks& start_ticks,
                 const base::Time& start_time,
                 std::optional<Cronet_DateTime>* out) {
  Cronet_DateTime date_time;
  date_time.value = metrics_util::ConvertTime(ticks, start_ticks, start_time);
  if (date_time.value == metrics_util::kNullTime) {
    (*out).reset();
    return;
  }
  (*out).emplace(date_time);
}

}  // namespace native_metrics_util

}  // namespace cronet
