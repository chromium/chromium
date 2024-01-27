// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_METRICS_FAKES_FAKE_SAMPLER_H_
#define COMPONENTS_REPORTING_METRICS_FAKES_FAKE_SAMPLER_H_

#include <optional>

#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting::test {

class FakeSampler : public Sampler {
 public:
  FakeSampler();

  FakeSampler(const FakeSampler& other) = delete;
  FakeSampler& operator=(const FakeSampler& other) = delete;

  ~FakeSampler() override;

  void MaybeCollect(OptionalMetricCallback cb) override;

  void SetMetricData(std::optional<MetricData> metric_data);

  int GetNumCollectCalls() const;

 protected:
  std::optional<MetricData> metric_data_;

  int num_calls_ = 0;
};

class FakeDelayedSampler : public FakeSampler {
 public:
  FakeDelayedSampler();

  FakeDelayedSampler(const FakeDelayedSampler& other) = delete;
  FakeDelayedSampler& operator=(const FakeDelayedSampler& other) = delete;

  ~FakeDelayedSampler() override;

  void MaybeCollect(OptionalMetricCallback cb) override;

  void RunCallback();

 private:
  OptionalMetricCallback cb_;
};

}  // namespace reporting::test

#endif  // COMPONENTS_REPORTING_METRICS_FAKES_FAKE_SAMPLER_H_
