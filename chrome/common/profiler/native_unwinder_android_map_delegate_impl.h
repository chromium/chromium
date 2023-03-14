// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_NATIVE_UNWINDER_ANDROID_MAP_DELEGATE_IMPL_H_
#define CHROME_COMMON_PROFILER_NATIVE_UNWINDER_ANDROID_MAP_DELEGATE_IMPL_H_

#include <stdint.h>
#include <memory>

#include "base/functional/callback.h"
#include "base/profiler/native_unwinder_android_map_delegate.h"
#include "base/profiler/native_unwinder_android_memory_regions_map.h"
#include "chrome/android/modules/stack_unwinder/public/module.h"

// The implementation of map delegate that manages the lifecycle of
// libunwindstack resources. It is intended that this logic lives in chrome code
// instead of in the stack unwinder dynamic feature module. A single instance of
// this class is expected to be there for each process. The Get/Release of
// libunwindstack resources should happen in the same thread, i.e. the sampling
// thread (`StackSamplingProfiler::SamplingThread`).
// Note: `StackSamplingProfiler::SamplingThread` will terminate itself on idle,
// and restarts when there is task. The restarted thread is considered a new
// thread with a different thread id. So we cannot use `base::SequenceChecker`
// to verify that its methods are always called in the same thread.
class NativeUnwinderAndroidMapDelegateImpl
    : public base::NativeUnwinderAndroidMapDelegate {
 public:
  explicit NativeUnwinderAndroidMapDelegateImpl(stack_unwinder::Module* module);

  ~NativeUnwinderAndroidMapDelegateImpl() override;

  base::NativeUnwinderAndroidMemoryRegionsMap* GetMapReference() override;

  void ReleaseMapReference() override;

 private:
  const raw_ptr<stack_unwinder::Module> module_;
  uint32_t reference_count_ = 0u;
  std::unique_ptr<base::NativeUnwinderAndroidMemoryRegionsMap>
      memory_regions_map_;
};

#endif  // CHROME_COMMON_PROFILER_NATIVE_UNWINDER_ANDROID_MAP_DELEGATE_IMPL_H_
