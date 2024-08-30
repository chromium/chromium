// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/switches.h"

namespace heap_profiling::switches {

// Forces Heap Profiling on for a subprocess. The browser will add this when
// the kHeapProfilerCentralControl feature is enabled and the subprocess should
// be profiled.
const char kSubprocessHeapProfiling[] = "subproc-heap-profiling";

}  // namespace heap_profiling::switches
