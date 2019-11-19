// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SINGLE_SAMPLE_METRICS_H_
#define COMPONENTS_METRICS_SINGLE_SAMPLE_METRICS_H_

#include "base/callback.h"
#include "components/metrics/public/mojom/single_sample_metrics.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace metrics {

using CreateProviderCB = base::RepeatingCallback<void(
    mojo::PendingReceiver<mojom::SingleSampleMetricsProvider>)>;

// Initializes and sets the base::SingleSampleMetricsFactory for the current
// process. |create_provider_cb| is used to create provider instances per each
// thread that the factory is used on; this is necessary since the underlying
// providers must only be used on the same thread as construction.
//
// We use a callback here to avoid taking additional DEPS on content and a
// service_manager::Connector() for simplicity and to avoid the need for
// using the service test harness in metrics unittests.
//
// Typically this is called in the process where termination may occur without
// warning; e.g. perhaps a renderer process.
extern void InitializeSingleSampleMetricsFactory(
    CreateProviderCB create_provider_cb);

// Creates a mojom::SingleSampleMetricsProvider capable of vending single sample
// metrics attached to a mojo pipe.
//
// Typically this is given to a service_manager::BinderRegistry in the process
// that has a deterministic shutdown path and which serves as a stable endpoint
// for the factory created by the above initialize method in another process.
extern void CreateSingleSampleMetricsProvider(
    mojo::PendingReceiver<mojom::SingleSampleMetricsProvider> receiver);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SINGLE_SAMPLE_METRICS_H_
