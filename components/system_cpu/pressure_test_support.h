// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_CPU_PRESSURE_TEST_SUPPORT_H_
#define COMPONENTS_SYSTEM_CPU_PRESSURE_TEST_SUPPORT_H_

#include <stdint.h>

#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/system_cpu/cpu_probe.h"
#include "components/system_cpu/cpu_sample.h"

namespace system_cpu {

// Test double for platform specific CpuProbe that stores the CpuSample in
// a TestFuture.
template <typename T,
          typename = std::enable_if_t<std::is_base_of_v<CpuProbe, T>>>
class FakePlatformCpuProbe : public T {
 public:
  template <typename... Args>
  explicit FakePlatformCpuProbe(Args&&... args)
      : T(std::forward<Args>(args)...) {}
  ~FakePlatformCpuProbe() override = default;

  // Tests the internals of each platform CPU probe by calling Update() directly
  // instead of using the public interface.
  std::optional<CpuSample> UpdateAndWaitForSample() {
    T::Update(sample_.GetCallback());
    // Blocks until the sample callback is invoked.
    return sample_.Take();
  }

 private:
  base::test::TestFuture<std::optional<CpuSample>> sample_;
};

// Test double for CpuProbe that always returns a predetermined value.
class FakeCpuProbe final : public CpuProbe {
 public:
  // Creates a FakeCpuProbe that delays Update() responses by `response_delay`.
  // Setting this to >0 can mimic production CpuProbes that take samples on
  // background threads.
  explicit FakeCpuProbe(base::TimeDelta response_delay = base::TimeDelta());

  ~FakeCpuProbe() final;

  // CpuProbe implementation.
  void Update(SampleCallback callback) final;
  base::WeakPtr<CpuProbe> GetWeakPtr() final;

  // Can be called from any thread.
  void SetLastSample(std::optional<CpuSample> sample);

 private:
  const base::TimeDelta response_delay_;

  base::Lock lock_;
  std::optional<CpuSample> last_sample_ GUARDED_BY_CONTEXT(lock_);

  base::WeakPtrFactory<FakeCpuProbe> weak_factory_{this};
};

// Test double for CpuProbe that produces a different value after every
// Update().
class StreamingCpuProbe final : public CpuProbe {
 public:
  StreamingCpuProbe(std::vector<CpuSample>, base::OnceClosure);

  ~StreamingCpuProbe() final;

  // CpuProbe implementation.
  void Update(SampleCallback callback) final;
  base::WeakPtr<CpuProbe> GetWeakPtr() final;

 private:
  std::vector<CpuSample> samples_ GUARDED_BY_CONTEXT(sequence_checker_);
  size_t sample_index_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // This closure is called on an Update() call after the expected number of
  // samples has been taken by CpuSampler.
  base::OnceClosure done_callback_;

  base::WeakPtrFactory<StreamingCpuProbe> weak_factory_{this};
};

}  // namespace system_cpu

#endif  // COMPONENTS_SYSTEM_CPU_PRESSURE_TEST_SUPPORT_H_
