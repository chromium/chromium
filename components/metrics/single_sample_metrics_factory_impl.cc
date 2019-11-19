// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/single_sample_metrics_factory_impl.h"

#include <memory>

#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace metrics {

namespace {

class SingleSampleMetricImpl : public base::SingleSampleMetric {
 public:
  SingleSampleMetricImpl(mojo::PendingRemote<mojom::SingleSampleMetric> metric)
      : metric_(std::move(metric)) {}

  ~SingleSampleMetricImpl() override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  void SetSample(base::HistogramBase::Sample sample) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    metric_->SetSample(sample);
  }

 private:
  base::ThreadChecker thread_checker_;
  mojo::Remote<mojom::SingleSampleMetric> metric_;

  DISALLOW_COPY_AND_ASSIGN(SingleSampleMetricImpl);
};

}  // namespace

SingleSampleMetricsFactoryImpl::SingleSampleMetricsFactoryImpl(
    CreateProviderCB create_provider_cb)
    : create_provider_cb_(std::move(create_provider_cb)) {}

SingleSampleMetricsFactoryImpl::~SingleSampleMetricsFactoryImpl() {}

std::unique_ptr<base::SingleSampleMetric>
SingleSampleMetricsFactoryImpl::CreateCustomCountsMetric(
    const std::string& histogram_name,
    base::HistogramBase::Sample min,
    base::HistogramBase::Sample max,
    uint32_t bucket_count) {
  return CreateMetric(histogram_name, min, max, bucket_count,
                      base::HistogramBase::kUmaTargetedHistogramFlag);
}

void SingleSampleMetricsFactoryImpl::DestroyProviderForTesting() {
  if (auto* provider = provider_tls_.Get())
    delete provider;
  provider_tls_.Set(nullptr);
}

std::unique_ptr<base::SingleSampleMetric>
SingleSampleMetricsFactoryImpl::CreateMetric(const std::string& histogram_name,
                                             base::HistogramBase::Sample min,
                                             base::HistogramBase::Sample max,
                                             uint32_t bucket_count,
                                             int32_t flags) {
  mojo::PendingRemote<mojom::SingleSampleMetric> metric;
  GetProvider()->AcquireSingleSampleMetric(
      histogram_name, min, max, bucket_count, flags,
      metric.InitWithNewPipeAndPassReceiver());
  return std::make_unique<SingleSampleMetricImpl>(std::move(metric));
}

mojom::SingleSampleMetricsProvider*
SingleSampleMetricsFactoryImpl::GetProvider() {
  // Check the current TLS slot to see if we have created a provider already for
  // this thread.
  if (auto* provider = provider_tls_.Get())
    return provider->get();

  // If not, create a new one which will persist until process shutdown and put
  // it in the TLS slot for the current thread.
  mojo::Remote<mojom::SingleSampleMetricsProvider>* provider =
      new mojo::Remote<mojom::SingleSampleMetricsProvider>();
  provider_tls_.Set(provider);

  // Start the provider connection and return it; it won't be fully connected
  // until later, but mojo will buffer all calls prior to completion.
  create_provider_cb_.Run(provider->BindNewPipeAndPassReceiver());
  return provider->get();
}

}  // namespace metrics
