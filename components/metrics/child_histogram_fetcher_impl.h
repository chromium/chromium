// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CHILD_HISTOGRAM_FETCHER_IMPL_H_
#define COMPONENTS_METRICS_CHILD_HISTOGRAM_FETCHER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "components/metrics/public/mojom/histogram_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class HistogramDeltaSerialization;
}  // namespace base

namespace metrics {

class ChildHistogramFetcherFactoryImpl
    : public mojom::ChildHistogramFetcherFactory {
 public:
  ChildHistogramFetcherFactoryImpl();
  ~ChildHistogramFetcherFactoryImpl() override;

  static void Create(
      mojo::PendingReceiver<mojom::ChildHistogramFetcherFactory>);

 private:
  // mojom::ChildHistogramFetcherFactory:
  void CreateFetcher(
      base::UnsafeSharedMemoryRegion,
      mojo::PendingReceiver<mojom::ChildHistogramFetcher>) override;
};

class ChildHistogramFetcherImpl : public mojom::ChildHistogramFetcher {
 public:
  ChildHistogramFetcherImpl();

  ChildHistogramFetcherImpl(const ChildHistogramFetcherImpl&) = delete;
  ChildHistogramFetcherImpl& operator=(const ChildHistogramFetcherImpl&) =
      delete;

  ~ChildHistogramFetcherImpl() override;

 private:
  // mojom::ChildHistogramFetcher:
  void GetChildNonPersistentHistogramData(
      GetChildNonPersistentHistogramDataCallback callback) override;
  void Ping(mojom::UmaPingCallSource call_source,
            PingCallback callback) override;

  // Extract snapshot data and then send it off to the Browser process.
  // Send only a delta to what we have already sent.
  void UploadAllHistograms(int64_t sequence_number);

  // Prepares histogram deltas for transmission.
  std::unique_ptr<base::HistogramDeltaSerialization>
      histogram_delta_serialization_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CHILD_HISTOGRAM_FETCHER_IMPL_H_
