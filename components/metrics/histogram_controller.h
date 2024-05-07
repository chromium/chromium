// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_HISTOGRAM_CONTROLLER_H_
#define COMPONENTS_METRICS_HISTOGRAM_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/metrics/histogram_child_process.h"
#include "components/metrics/public/mojom/histogram_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace metrics {

class HistogramSubscriber;

// HistogramController is used on the browser process to collect histogram data.
// Only one thread (typically the UI thread) is allowed to interact with the
// HistogramController object.
class COMPONENT_EXPORT(METRICS) HistogramController {
 public:
  // Returns the HistogramController object for the current process, or null if
  // none.
  static HistogramController* GetInstance();

  // Normally instantiated when the child process is launched. Only one instance
  // should be created per process.
  HistogramController();

  HistogramController(const HistogramController&) = delete;
  HistogramController& operator=(const HistogramController&) = delete;

  virtual ~HistogramController();

  // Register the subscriber so that it will be called when for example
  // OnHistogramDataCollected is returning histogram data from a child process.
  void Register(HistogramSubscriber* subscriber);

  // Unregister the subscriber so that it will not be called when for example
  // OnHistogramDataCollected is returning histogram data from a child process.
  // Safe to call even if caller is not the current subscriber.
  void Unregister(const HistogramSubscriber* subscriber);

  // Contact all processes and get their histogram data.
  void GetHistogramData(int sequence_number);

  enum class ChildProcessMode {
    // This child process should be included when gathering non-persistent
    // histogram data from child processes.
    kGetHistogramData,

    // This child process should only be included in pings, but histogram data
    // should not be collected.
    kPingOnly,
  };
  void SetHistogramMemory(HistogramChildProcess* host,
                          base::UnsafeSharedMemoryRegion shared_region,
                          ChildProcessMode mode);

  // Some hosts can be re-used before Mojo recognizes that their connections
  // are invalid because the previous child process died.
  void NotifyChildDied(HistogramChildProcess* host);

 private:
  friend struct base::LeakySingletonTraits<HistogramController>;

  raw_ptr<HistogramSubscriber> subscriber_;

  void InsertChildHistogramFetcherInterface(
      HistogramChildProcess* host,
      mojo::Remote<mojom::ChildHistogramFetcher> child_histogram_fetcher,
      ChildProcessMode mode);

  // Calls PingChildProcess() on ~10% of child processes. Not all child
  // processes are pinged so as to avoid possibly "waking up" too many and
  // causing unnecessary work.
  void PingChildProcesses();

  // Pings a child process through its |fetcher|. This does nothing except emit
  // histograms (both on the browser process and the child process), with the
  // goal of quantifying the amount of histogram samples lost from child
  // processes.
  void PingChildProcess(mojom::ChildHistogramFetcherProxy* fetcher,
                        mojom::UmaPingCallSource call_source);

  // Callback for when a child process has received a ping (see
  // PingChildProcess()).
  void Pong(mojom::UmaPingCallSource call_source);

  void RemoveChildHistogramFetcherInterface(
      MayBeDangling<HistogramChildProcess> host);

  // Records the histogram data collected from a child process.
  void OnHistogramDataCollected(
      int sequence_number,
      const std::vector<std::string>& pickled_histograms);

  struct ChildHistogramFetcher;
  using ChildHistogramFetcherMap =
      std::map<HistogramChildProcess*, ChildHistogramFetcher>;
  ChildHistogramFetcherMap child_histogram_fetchers_;

  // Used to call PingAllChildProcesses() every 5 minutes.
  base::RepeatingTimer timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_HISTOGRAM_CONTROLLER_H_
