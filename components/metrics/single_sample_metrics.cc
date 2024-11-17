// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/single_sample_metrics.h"

#include <memory>
#include <utility>

#include "base/metrics/single_sample_metrics.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/single_sample_metrics_factory_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace metrics {
namespace {

class MojoSingleSampleMetric : public mojom::SingleSampleMetric {
 public:
  MojoSingleSampleMetric(const std::string& histogram_name,
                         base::HistogramBase::Sample min,
                         base::HistogramBase::Sample max,
                         uint32_t bucket_count,
                         int32_t flags)
      : metric_(histogram_name, min, max, bucket_count, flags) {}

  MojoSingleSampleMetric(const MojoSingleSampleMetric&) = delete;
  MojoSingleSampleMetric& operator=(const MojoSingleSampleMetric&) = delete;

  ~MojoSingleSampleMetric() override = default;

 private:
  // mojom::SingleSampleMetric:
  void SetSample(base::HistogramBase::Sample sample) override {
    metric_.SetSample(sample);
  }

  base::DefaultSingleSampleMetric metric_;
};

class MojoSingleSampleMetricsProvider
    : public mojom::SingleSampleMetricsProvider {
 public:
  MojoSingleSampleMetricsProvider() = default;

  MojoSingleSampleMetricsProvider(const MojoSingleSampleMetricsProvider&) =
      delete;
  MojoSingleSampleMetricsProvider& operator=(
      const MojoSingleSampleMetricsProvider&) = delete;

  ~MojoSingleSampleMetricsProvider() override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

 private:
  // mojom::SingleSampleMetricsProvider:
  void AcquireSingleSampleMetric(
      const std::string& histogram_name,
      base::HistogramBase::Sample min,
      base::HistogramBase::Sample max,
      uint32_t bucket_count,
      int32_t flags,
      mojo::PendingReceiver<mojom::SingleSampleMetric> receiver) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MojoSingleSampleMetric>(histogram_name, min, max,
                                                 bucket_count, flags),
        std::move(receiver));
  }

  // Providers must be created, used on, and destroyed on the same thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace

// static
void InitializeSingleSampleMetricsFactory(CreateProviderCB create_provider_cb) {
  base::SingleSampleMetricsFactory::SetFactory(
      std::make_unique<SingleSampleMetricsFactoryImpl>(
          std::move(create_provider_cb)));
}

// static
void CreateSingleSampleMetricsProvider(
    mojo::PendingReceiver<mojom::SingleSampleMetricsProvider> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MojoSingleSampleMetricsProvider>(), std::move(receiver));
}

}  // namespace metrics
