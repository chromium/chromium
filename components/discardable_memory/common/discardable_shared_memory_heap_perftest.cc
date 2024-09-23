// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/common/discardable_shared_memory_heap.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/page_size.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace discardable_memory {
namespace {

const int kTimeLimitMs = 2000;
const int kTimeCheckInterval = 8192;

void NullTask() {}

TEST(DiscardableSharedMemoryHeapTest, SearchFreeLists) {
  DiscardableSharedMemoryHeap heap;

  const size_t kBlocks = 4096;
  const size_t kSegments = 16;
  size_t segment_size = base::GetPageSize() * kBlocks;
  int next_discardable_shared_memory_id = 0;

  for (size_t i = 0; i < kSegments; ++i) {
    std::unique_ptr<base::DiscardableSharedMemory> memory(
        new base::DiscardableSharedMemory);
    ASSERT_TRUE(memory->CreateAndMap(segment_size));
    heap.MergeIntoFreeLists(heap.Grow(std::move(memory), segment_size,
                                      next_discardable_shared_memory_id++,
                                      base::BindOnce(NullTask)));
  }

  unsigned kSeed = 1;
  // Use kSeed as seed for random number generator.
  srand(kSeed);

  // Pre-compute random values.
  std::array<int, kTimeCheckInterval> random_span;
  std::array<size_t, kTimeCheckInterval> random_blocks;
  for (int i = 0; i < kTimeCheckInterval; ++i) {
    random_span[i] = std::rand();
    // Exponentially distributed block size.
    const double kLambda = 2.0;
    double v = static_cast<double>(std::rand()) / RAND_MAX;
    random_blocks[i] = 1 + log(1.0 - v) / -kLambda * kBlocks;
  }

  std::vector<std::unique_ptr<base::ScopedClosureRunner>> spans;

  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks end = start + base::Milliseconds(kTimeLimitMs);
  base::TimeDelta accumulator;
  int count = 0;
  while (start < end) {
    for (int i = 0; i < kTimeCheckInterval; ++i) {
      // Search for a perfect fit if greater than kBlocks.
      size_t slack =
          random_blocks[i] < kBlocks ? kBlocks - random_blocks[i] : 0;
      std::unique_ptr<DiscardableSharedMemoryHeap::Span> span =
          heap.SearchFreeLists(random_blocks[i], slack);
      if (span) {
        spans.push_back(std::make_unique<base::ScopedClosureRunner>(
            base::BindOnce(&DiscardableSharedMemoryHeap::MergeIntoFreeLists,
                           base::Unretained(&heap), std::move(span))));
      } else if (!spans.empty()) {
        // Merge a random span back into the free list.
        std::swap(spans[random_span[i] % spans.size()], spans.back());
        spans.pop_back();
      }

      ++count;
    }

    base::TimeTicks now = base::TimeTicks::Now();
    accumulator += now - start;
    start = now;
  }

  spans.clear();

  perf_test::PerfResultReporter reporter("DiscardableSharedMemoryHeap.",
                                         "search_free_list");
  reporter.RegisterImportantMetric("throughput", "runs/s");
  reporter.AddResult("throughput", count / accumulator.InSecondsF());
}

}  // namespace
}  // namespace discardable_memory
