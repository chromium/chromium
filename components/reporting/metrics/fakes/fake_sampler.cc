// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/fakes/fake_sampler.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting::test {

FakeSampler::FakeSampler() = default;

FakeSampler::~FakeSampler() = default;

void FakeSampler::MaybeCollect(OptionalMetricCallback cb) {
  ++num_calls_;
  std::move(cb).Run(metric_data_);
}

void FakeSampler::SetMetricData(std::optional<MetricData> metric_data) {
  metric_data_ = std::move(metric_data);
}

int FakeSampler::GetNumCollectCalls() const {
  return num_calls_;
}

FakeDelayedSampler::FakeDelayedSampler() = default;

FakeDelayedSampler::~FakeDelayedSampler() = default;

void FakeDelayedSampler::MaybeCollect(OptionalMetricCallback cb) {
  ++num_calls_;
  cb_ = std::move(cb);
}

void FakeDelayedSampler::RunCallback() {
  CHECK(cb_);
  std::move(cb_).Run(metric_data_);
}

}  // namespace reporting::test
