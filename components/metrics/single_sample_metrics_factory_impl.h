// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SINGLE_VALUE_HISTOGRAM_FACTORY_IMPL_H_
#define COMPONENTS_METRICS_SINGLE_VALUE_HISTOGRAM_FACTORY_IMPL_H_

#include <string>

#include "base/metrics/single_sample_metrics.h"
#include "base/threading/thread_local.h"
#include "components/metrics/public/mojom/single_sample_metrics.mojom.h"
#include "components/metrics/single_sample_metrics.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace metrics {

// SingleSampleMetricsFactory implementation for creating SingleSampleMetric
// instances that communicate over mojo to instances in another process.
//
// Persistance outside of the current process allows these metrics to record a
// sample even in the event of sudden process termination. As an example, this
// is useful for garbage collected objects which may never get a chance to run
// their destructors in the event of a fast shutdown event (process kill).
class SingleSampleMetricsFactoryImpl : public base::SingleSampleMetricsFactory {
 public:
  // Constructs a factory capable of vending single sample metrics from any
  // thread. |create_provider_cb| will be called from arbitrary threads to
  // create providers as necessary; the callback must handle thread safety.
  //
  // We use a callback here to avoid taking additional DEPS on content and a
  // service_manager::Connector() for simplicitly and to avoid the need for
  // using the service test harness just for instantiating this class.
  explicit SingleSampleMetricsFactoryImpl(CreateProviderCB create_provider_cb);
  ~SingleSampleMetricsFactoryImpl() override;

  // base::SingleSampleMetricsFactory:
  std::unique_ptr<base::SingleSampleMetric> CreateCustomCountsMetric(
      const std::string& histogram_name,
      base::HistogramBase::Sample min,
      base::HistogramBase::Sample max,
      uint32_t bucket_count) override;

  // Providers live forever in production, but tests should be kind and clean up
  // after themselves to avoid tests trampling on one another. Destroys the
  // provider in the TLS slot for the calling thread.
  void DestroyProviderForTesting();

 private:
  // Creates a single sample metric.
  std::unique_ptr<base::SingleSampleMetric> CreateMetric(
      const std::string& histogram_name,
      base::HistogramBase::Sample min,
      base::HistogramBase::Sample max,
      uint32_t bucket_count,
      int32_t flags);

  // Gets the SingleSampleMetricsProvider for the current thread. If none
  // exists, then a new instance is created and set in the TLS slot.
  mojom::SingleSampleMetricsProvider* GetProvider();

  CreateProviderCB create_provider_cb_;

  // Per thread storage slot for the mojo provider.
  base::ThreadLocalPointer<mojo::Remote<mojom::SingleSampleMetricsProvider>>
      provider_tls_;

  DISALLOW_COPY_AND_ASSIGN(SingleSampleMetricsFactoryImpl);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SINGLE_VALUE_HISTOGRAM_FACTORY_IMPL_H_
