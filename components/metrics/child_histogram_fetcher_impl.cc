// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/child_histogram_fetcher_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_delta_serialization.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "components/metrics/public/mojom/histogram_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace metrics {

ChildHistogramFetcherFactoryImpl::ChildHistogramFetcherFactoryImpl() = default;

ChildHistogramFetcherFactoryImpl::~ChildHistogramFetcherFactoryImpl() = default;

void ChildHistogramFetcherFactoryImpl::Create(
    mojo::PendingReceiver<mojom::ChildHistogramFetcherFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ChildHistogramFetcherFactoryImpl>(),
      std::move(receiver));
}

void ChildHistogramFetcherFactoryImpl::CreateFetcher(
    base::UnsafeSharedMemoryRegion shared_memory,
    mojo::PendingReceiver<mojom::ChildHistogramFetcher> receiver) {
  // This message must be received only once.
  static bool already_called = false;
  CHECK(!already_called);
  already_called = true;

  // If the shared memory region was passed via the command line, then the
  // global histogram allocator should already by setup. Otherwise, the region
  // is being passed via this IPC. We need to initialize the global histogram
  // allocator and the tracking histograms here.
  if (shared_memory.IsValid() && !base::GlobalHistogramAllocator::Get()) {
    base::GlobalHistogramAllocator::CreateWithSharedMemoryRegion(shared_memory);
    // Emit a local histogram, which should not be reported to servers. This is
    // monitored from the serverside.
    LOCAL_HISTOGRAM_BOOLEAN("UMA.LocalHistogram", true);

    base::PersistentHistogramAllocator* global_allocator =
        base::GlobalHistogramAllocator::Get();
    if (global_allocator) {
      global_allocator->CreateTrackingHistograms(global_allocator->Name());
    }
  }

  // Setup the Mojo receiver.
  mojo::MakeSelfOwnedReceiver(std::make_unique<ChildHistogramFetcherImpl>(),
                              std::move(receiver));
}

ChildHistogramFetcherImpl::ChildHistogramFetcherImpl() = default;

ChildHistogramFetcherImpl::~ChildHistogramFetcherImpl() = default;

// Extract snapshot data and then send it off to the Browser process.
// Send only a delta to what we have already sent.
void ChildHistogramFetcherImpl::GetChildNonPersistentHistogramData(
    GetChildNonPersistentHistogramDataCallback callback) {
  // If a persistent allocator is in use, it needs to occasionally update
  // some internal histograms. An upload is happening so this is a good time.
  base::PersistentHistogramAllocator* global_allocator =
      base::GlobalHistogramAllocator::Get();
  if (global_allocator) {
    global_allocator->UpdateTrackingHistograms();
  }

  if (!histogram_delta_serialization_) {
    histogram_delta_serialization_ =
        std::make_unique<base::HistogramDeltaSerialization>("ChildProcess");
  }

  std::vector<std::string> deltas;
  // "false" to PerpareAndSerializeDeltas() indicates to *not* include
  // histograms held in persistent storage on the assumption that they will be
  // visible to the recipient through other means.
  histogram_delta_serialization_->PrepareAndSerializeDeltas(&deltas, false);

  std::move(callback).Run(deltas);

#ifndef NDEBUG
  static int count = 0;
  count++;
  LOCAL_HISTOGRAM_COUNTS("Histogram.ChildProcessHistogramSentCount", count);
#endif
}

void ChildHistogramFetcherImpl::Ping(mojom::UmaPingCallSource call_source,
                                     PingCallback callback) {
  // Since the ChildHistogramFetcherImpl instance was created after setting up
  // the shared memory (if there was one -- see CreateFetcher()), this histogram
  // will live in it (i.e., it should have the |kIsPersistent| flag).
  const char* histogram_name = nullptr;
  switch (call_source) {
    case mojom::UmaPingCallSource::PERIODIC:
      histogram_name = "UMA.ChildProcess.Ping.Periodic";
      break;
    case mojom::UmaPingCallSource::SHARED_MEMORY_SET_UP:
      histogram_name = "UMA.ChildProcess.Ping.SharedMemorySetUp";
      break;
  }
  base::UmaHistogramEnumeration(histogram_name,
                                mojom::UmaChildPingStatus::CHILD_RECEIVED_IPC);

  std::move(callback).Run();
}

}  // namespace metrics
