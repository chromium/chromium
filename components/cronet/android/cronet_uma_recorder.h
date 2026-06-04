// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_UMA_RECORDER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_UMA_RECORDER_H_

#include <deque>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace cronet {

// Thread-safe recorder for Cronet UMA samples.
// Collects samples on any thread and reports them to Java via JNI
// on a background thread, applying rate limiting to prevent statsD overload.
class CronetUmaRecorder final {
 public:
  // Initializes the singleton with the given allowlist.
  // Must be called exactly once at startup (typically from JNI InitNative).
  //
  // The `allowlist` parameter specifies which UMA histograms are allowed to be
  // recorded. Format options:
  // 1. "*" : Allows all histograms (disables filtering).
  // 2. "hash1,hash2,..." : A comma-separated list of decimal uint64_t name
  // hashes.
  //    Only histograms whose name hashes match the list will be recorded.
  //    Example: "2937041049411630354,8946698020320526722"
  static void InitializeWithAllowlist(const std::string& allowlist);

  // Must be called after Initialize.
  static CronetUmaRecorder& GetInstance();

  CronetUmaRecorder(const CronetUmaRecorder&) = delete;
  CronetUmaRecorder& operator=(const CronetUmaRecorder&) = delete;

  // Request to log a sample. Thread-safe.
  void AddSample(uint64_t name_hash, int32_t value);

  ~CronetUmaRecorder();

  CronetUmaRecorder(base::PassKey<CronetUmaRecorder>,
                    const std::string& allowlist);

 private:
  // Processes pending UMA samples in the queue as much as the rate
  // limit allows.
  void ProcessQueue();

  // Checks if a name hash is allowed to be reported based on the allowlist.
  bool IsHashAllowed(uint64_t name_hash) const;

  // UMA filter allowlist. If nullopt, all hashes are allowed.
  const std::optional<absl::flat_hash_set<uint64_t>> allowed_name_hashes_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  struct Sample final {
    uint64_t hash;
    int32_t value;
  };

  base::Lock lock_;

  // Synchronized queue of UMA samples pending processing.
  std::deque<Sample> queue_ GUARDED_BY(lock_);

  // Maximum number of pending UMA samples in the queue. Used for load shedding
  // to prevent OOMs during extreme telemetry bursts. 5000 pending samples
  // represents ~5 seconds of maximum rate-limited throughput.
  static constexpr size_t kMaxPendingTasks = 5000;

  // Rate limiting constants.
  // StatsD starts dropping atoms if it receives more than 240 samples in less
  // than 100ms (using an internal 100ms fixed window).
  //
  // To prevent drops without the complexity and memory overhead of a
  // client-side sliding window (which would require tracking timestamps of all
  // recent samples), we use a simpler client-side Fixed Window rate limiter.
  //
  // Because the client and StatsD windows are not synchronized, they can be out
  // of phase. In the worst case (boundary burst), a client-side fixed window
  // could allow up to 2 * Limit samples to arrive within a single StatsD
  // window.
  //
  // To guarantee we never exceed StatsD's 240 limit even during a boundary
  // burst, halve the client-side limit to 100 samples per
  // 100ms. This ensures that at most 200 samples (100 at the end of window N +
  // 100 at the start of window N+1) are ever sent in any 100ms period, keeping
  // us safely under the StatsD threshold while keeping the implementation
  // extremely simple.
  static constexpr size_t kMaxSamplesPerPeriod = 100;
  static constexpr base::TimeDelta kRateLimitPeriod = base::Milliseconds(100);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_UMA_RECORDER_H_
