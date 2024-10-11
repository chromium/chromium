// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAMPLING_PROFILER_PROCESS_TYPE_H_
#define COMPONENTS_SAMPLING_PROFILER_PROCESS_TYPE_H_

namespace sampling_profiler {

// The type of process which is profiled.
enum class ProfilerProcessType {
  kUnknown,
  kBrowser,
  kRenderer,
  kGpu,
  kUtility,
  kZygote,
  kSandboxHelper,
  kPpapiPlugin,
  kNetworkService,

  kMax = kNetworkService,
};

// The type of thread which is profiled.
enum class ProfilerThreadType {
  kUnknown,

  // Each process has a 'main thread'. In the Browser process, the 'main
  // thread' is also often called the 'UI thread'.
  kMain,
  kIo,

  // Compositor thread (can be in both renderer and gpu processes).
  kCompositor,

  // Service worker thread.
  kServiceWorker,

  kMax = kServiceWorker,
};

}  // namespace sampling_profiler

#endif  // COMPONENTS_SAMPLING_PROFILER_PROCESS_TYPE_H_
