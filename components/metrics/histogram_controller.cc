// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/histogram_controller.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "components/metrics/histogram_subscriber.h"
#include "components/metrics/public/mojom/histogram_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace metrics {

namespace {
const char* GetPingHistogramName(mojom::UmaPingCallSource call_source) {
  switch (call_source) {
    case mojom::UmaPingCallSource::PERIODIC:
      return "UMA.ChildProcess.Ping.Periodic";
    case mojom::UmaPingCallSource::SHARED_MEMORY_SET_UP:
      return "UMA.ChildProcess.Ping.SharedMemorySetUp";
  }
}
}  // namespace

struct HistogramController::ChildHistogramFetcher {
  mojo::Remote<mojom::ChildHistogramFetcher> remote;
  ChildProcessMode mode;
};

HistogramController* HistogramController::GetInstance() {
  return base::Singleton<HistogramController, base::LeakySingletonTraits<
                                                  HistogramController>>::get();
}

HistogramController::HistogramController() : subscriber_(nullptr) {
  // Unretained is safe because |this| is leaky.
  timer_.Start(FROM_HERE, base::Minutes(5),
               base::BindRepeating(&HistogramController::PingChildProcesses,
                                   base::Unretained(this)));
}

HistogramController::~HistogramController() = default;

void HistogramController::Register(HistogramSubscriber* subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!subscriber_);
  subscriber_ = subscriber;
}

void HistogramController::Unregister(const HistogramSubscriber* subscriber) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(subscriber_, subscriber);
  subscriber_ = nullptr;
}

void HistogramController::NotifyChildDied(HistogramChildProcess* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveChildHistogramFetcherInterface(
      MayBeDangling<HistogramChildProcess>(host));
}

void HistogramController::SetHistogramMemory(
    HistogramChildProcess* host,
    base::UnsafeSharedMemoryRegion shared_region,
    ChildProcessMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<mojom::ChildHistogramFetcherFactory> factory;
  host->BindChildHistogramFetcherFactory(factory.BindNewPipeAndPassReceiver());

  mojo::Remote<mojom::ChildHistogramFetcher> fetcher;
  factory->CreateFetcher(std::move(shared_region),
                         fetcher.BindNewPipeAndPassReceiver());
  PingChildProcess(fetcher.get(),
                   mojom::UmaPingCallSource::SHARED_MEMORY_SET_UP);
  InsertChildHistogramFetcherInterface(host, std::move(fetcher), mode);
}

void HistogramController::InsertChildHistogramFetcherInterface(
    HistogramChildProcess* host,
    mojo::Remote<mojom::ChildHistogramFetcher> child_histogram_fetcher,
    ChildProcessMode mode) {
  // Broken pipe means remove this from the map. The map size is a proxy for
  // the number of known processes
  //
  // `RemoveChildHistogramFetcherInterface` will only use `host` for address
  // comparison without being dereferenced , therefore it's not going to create
  // a UAF.
  child_histogram_fetcher.set_disconnect_handler(
      base::BindOnce(&HistogramController::RemoveChildHistogramFetcherInterface,
                     base::Unretained(this), base::UnsafeDangling(host)));
  child_histogram_fetchers_.emplace(
      host, ChildHistogramFetcher{std::move(child_histogram_fetcher), mode});
}

void HistogramController::PingChildProcesses() {
  // Only ping ~10% of child processes to avoid possibly "waking up" too many
  // and causing unnecessary work.
  for (const auto& fetcher : child_histogram_fetchers_) {
    if (base::RandGenerator(/*range=*/10) == 0) {
      PingChildProcess(fetcher.second.remote.get(),
                       mojom::UmaPingCallSource::PERIODIC);
    }
  }
}

void HistogramController::PingChildProcess(
    mojom::ChildHistogramFetcherProxy* fetcher,
    mojom::UmaPingCallSource call_source) {
  // 1) Emit a histogram, 2) ping the child process (which should also emit a
  // histogram), and 3) call Pong(), which again emits a histogram.
  // If no histograms are lost, in total, the histograms should all be emitted
  // roughly the same amount of times. The exception is for 1), which may be
  // emitted more often because this may be called early on in the lifecycle of
  // the child process, and some child processes are killed very early on,
  // before any IPC messages are processed.
  base::UmaHistogramEnumeration(GetPingHistogramName(call_source),
                                mojom::UmaChildPingStatus::BROWSER_SENT_IPC);
  // Unretained is safe because |this| is leaky.
  fetcher->Ping(call_source,
                base::BindOnce(&HistogramController::Pong,
                               base::Unretained(this), call_source));
}

void HistogramController::Pong(mojom::UmaPingCallSource call_source) {
  base::UmaHistogramEnumeration(
      GetPingHistogramName(call_source),
      mojom::UmaChildPingStatus::BROWSER_REPLY_CALLBACK);
}

void HistogramController::RemoveChildHistogramFetcherInterface(
    MayBeDangling<HistogramChildProcess> host) {
  child_histogram_fetchers_.erase(host);
}

void HistogramController::GetHistogramData(int sequence_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int pending_processes = 0;
  for (const auto& fetcher : child_histogram_fetchers_) {
    if (fetcher.second.mode != ChildProcessMode::kGetHistogramData) {
      continue;
    }

    fetcher.second.remote->GetChildNonPersistentHistogramData(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&HistogramController::OnHistogramDataCollected,
                           base::Unretained(this), sequence_number),
            std::vector<std::string>()));
    ++pending_processes;
  }

  if (subscriber_) {
    subscriber_->OnPendingProcesses(sequence_number, pending_processes, true);
  }
}

void HistogramController::OnHistogramDataCollected(
    int sequence_number,
    const std::vector<std::string>& pickled_histograms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (subscriber_) {
    subscriber_->OnHistogramDataCollected(sequence_number, pickled_histograms);
  }
}

}  // namespace metrics
