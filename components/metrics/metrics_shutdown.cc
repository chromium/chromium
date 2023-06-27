// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_shutdown.h"

#include "base/metrics/persistent_histogram_allocator.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/prefs/pref_service.h"

namespace metrics {

void Shutdown(PrefService* local_state) {
  CleanExitBeacon::EnsureCleanShutdown(local_state);
  auto* allocator = base::GlobalHistogramAllocator::Get();
  if (allocator) {
    // Write to the persistent histogram allocator that we've exited cleanly.
    // This is not perfect as the browser may crash after this call (or the
    // device may lose power, which may leave the underlying pages in an
    // inconsistent state), but this is "good" enough for debugging sake.
    allocator->memory_allocator()->SetMemoryState(
        base::PersistentMemoryAllocator::MemoryState::MEMORY_COMPLETED);
  }
}

}  // namespace metrics
