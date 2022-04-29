// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/sampler.h"

#include <utility>

#include "base/bind.h"

namespace reporting {

void Sampler::MaybeCollect(OptionalMetricCallback callback) {
  Collect(base::BindOnce(
      [](OptionalMetricCallback callback, MetricData metric_data) {
        std::move(callback).Run(std::move(metric_data));
      },
      std::move(callback)));
}

}  // namespace reporting
