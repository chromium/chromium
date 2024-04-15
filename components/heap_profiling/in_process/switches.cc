// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/switches.h"

namespace heap_profiling::switches {

// Forces Heap Profiling off for a subprocess. The browser will add this when
// the kHeapProfilerCentralControl feature is enabled but the subprocess should
// not be profiled.
// TODO(https://crbug.com/40840943): This is the default behaviour so the switch
// only exists to validate that HeapProfilerController adds a switch to every
// child process that's launched. Remove it once that's verified.
const char kNoSubprocessHeapProfiling[] = "no-subproc-heap-profiling";

// Forces Heap Profiling on for a subprocess. The browser will add this when
// the kHeapProfilerCentralControl feature is enabled and the subprocess should
// be profiled.
const char kSubprocessHeapProfiling[] = "subproc-heap-profiling";

}  // namespace heap_profiling::switches
