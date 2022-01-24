// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/fake_sampler.h"

namespace reporting {
namespace test {

void FakeSampler::Collect(MetricCallback cb) {
  num_calls_++;
  std::move(cb).Run(std::move(metric_data_));
}

void FakeSampler::SetMetricData(MetricData metric_data) {
  metric_data_ = std::move(metric_data);
}

int FakeSampler::GetNumCollectCalls() const {
  return num_calls_;
}
}  // namespace test
}  // namespace reporting
