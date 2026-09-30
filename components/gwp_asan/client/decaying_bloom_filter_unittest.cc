// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/decaying_bloom_filter.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gwp_asan::internal {
namespace {

constexpr size_t kTestNumCounters = 64;
constexpr size_t kTestNumHashes = 2;
constexpr size_t kTestDecaySteps = 8;

using TestFilter =
    DecayingBloomFilter<kTestNumCounters, kTestNumHashes, kTestDecaySteps>;
using FakeHashFilter =
    DecayingBloomFilter<kTestNumCounters,
                        kTestNumHashes,
                        kTestDecaySteps,
                        /*UseFakeHashFunctionsForTesting=*/true>;

// With the fake hash function, the hash equals the key, so `MakeKey(slot0,
// slot1)` builds a key whose N-th hash slice maps to `slotN`.
constexpr uint64_t MakeKey(size_t slot0, size_t slot1) {
  return slot0 | (slot1 << FakeHashFilter::kShiftWidth);
}

// | Key      | N=0 | N=1 |
// |----------+-----+-----|
// | kAlfa    |  16 |   8 |
// | kBravo   |  17 |   8 |  (shares slot 8 with kAlfa)
// | kCharlie |  32 |  16 |  (shares slot 16 with kAlfa)
// | kDelta   |   4 |   2 |  (disjoint from kAlfa, kBravo, kCharlie)
// | kEcho    |   1 |   0 |  (disjoint from all above)
constexpr uint64_t kAlfa = MakeKey(16, 8);
constexpr uint64_t kBravo = MakeKey(17, 8);
constexpr uint64_t kCharlie = MakeKey(32, 16);
constexpr uint64_t kDelta = MakeKey(4, 2);
constexpr uint64_t kEcho = MakeKey(1, 0);

TEST(DecayingBloomFilterTest, EmptyFilterContainsNothing) {
  TestFilter filter;
  EXPECT_EQ(filter.CountNonZeroCountersForTesting(), 0u);
  EXPECT_FALSE(filter.Contains(0));
  EXPECT_FALSE(filter.Contains(1));
  EXPECT_FALSE(filter.Contains(42));
}

TEST(DecayingBloomFilterTest, AddAndContainsSingleKey) {
  TestFilter filter;

  filter.Add(0);
  EXPECT_TRUE(filter.Contains(0));
  // `base::FastHash` uses a per-process random seed in DCHECK builds, so the
  // `kTestNumHashes` hash slices may collide on the same counter slot.
  EXPECT_GT(filter.CountNonZeroCountersForTesting(), 0u);
  EXPECT_LE(filter.CountNonZeroCountersForTesting(), kTestNumHashes);

  filter.Add(12345);
  EXPECT_TRUE(filter.Contains(12345));
  EXPECT_TRUE(filter.Contains(0));
}

TEST(DecayingBloomFilterTest, EvictionAfterDecaySteps) {
  FakeHashFilter filter;

  // Insert kDelta at step 0 (occupies slots {4, 2}).
  filter.Add(kDelta);
  EXPECT_TRUE(filter.Contains(kDelta));

  // Insert kTestDecaySteps - 1 entries of kCharlie (occupies slots {32, 16}).
  // kDelta should remain in the filter.
  for (size_t i = 0; i < kTestDecaySteps - 1; ++i) {
    filter.Add(kCharlie);
    EXPECT_TRUE(filter.Contains(kDelta));
  }

  // One more addition evicts kDelta.
  filter.Add(kCharlie);
  EXPECT_FALSE(filter.Contains(kDelta));
  EXPECT_TRUE(filter.Contains(kCharlie));
}

TEST(DecayingBloomFilterTest, SharedCounterReferenceCounting) {
  FakeHashFilter filter;

  // kAlfa sets slots {16, 8}.
  filter.Add(kAlfa);
  EXPECT_EQ(filter.CountNonZeroCountersForTesting(), 2u);
  EXPECT_TRUE(filter.Contains(kAlfa));
  EXPECT_FALSE(filter.Contains(kBravo));

  // kBravo sets slots {17, 8}. Slot 8 now has count 2.
  filter.Add(kBravo);
  EXPECT_EQ(filter.CountNonZeroCountersForTesting(), 3u);
  EXPECT_TRUE(filter.Contains(kAlfa));
  EXPECT_TRUE(filter.Contains(kBravo));

  // Push kTestDecaySteps - 1 disjoint entries (kDelta: slots {4, 2}).
  // This evicts kAlfa, decrementing slots {16, 8}.
  for (size_t i = 0; i < kTestDecaySteps - 1; ++i) {
    filter.Add(kDelta);
  }

  // kAlfa should now be evicted, but kBravo must still be present.
  EXPECT_FALSE(filter.Contains(kAlfa));
  EXPECT_TRUE(filter.Contains(kBravo));
}

constexpr size_t kConcurrentDecaySteps = 512;
using ConcurrentTestFilter =
    DecayingBloomFilter<kTestNumCounters,
                        kTestNumHashes,
                        kConcurrentDecaySteps,
                        /*UseFakeHashFunctionsForTesting=*/true>;

class AddWorker : public base::SimpleThread {
 public:
  AddWorker(ConcurrentTestFilter& filter, uint64_t key, size_t additions_count)
      : SimpleThread("BloomFilterAddWorker"),
        filter_(filter),
        key_(key),
        additions_count_(additions_count) {}

  void Run() override {
    for (size_t i = 0; i < additions_count_; ++i) {
      filter_->Add(key_);
    }
  }

 private:
  const raw_ref<ConcurrentTestFilter> filter_;
  const uint64_t key_;
  const size_t additions_count_;
};

TEST(DecayingBloomFilterTest, ConcurrentAddAndContains) {
  ConcurrentTestFilter filter;

  // Add `kEcho` twice before starting worker threads.
  filter.Add(kEcho);
  filter.Add(kEcho);
  EXPECT_TRUE(filter.Contains(kEcho));
  EXPECT_EQ(filter.GetCounterForTesting(1), 2u);
  EXPECT_EQ(filter.GetCounterForTesting(0), 2u);

  // 4 threads * 128 additions = 512 additions (`kConcurrentDecaySteps`).
  // These 512 additions fill the ring buffer and evict the two `kEcho` entries.
  constexpr size_t kAdditionsPerThread = 128;
  constexpr auto kKeys =
      std::to_array<uint64_t>({kAlfa, kBravo, kCharlie, kDelta});

  std::vector<std::unique_ptr<AddWorker>> threads;
  threads.reserve(kKeys.size());
  for (uint64_t key : kKeys) {
    threads.push_back(
        std::make_unique<AddWorker>(filter, key, kAdditionsPerThread));
    threads.back()->Start();
  }

  for (auto& thread : threads) {
    thread->Join();
  }

  // Verify `kEcho` was evicted and its counters returned to 0:
  EXPECT_FALSE(filter.Contains(kEcho));
  EXPECT_EQ(filter.GetCounterForTesting(1), 0u);
  EXPECT_EQ(filter.GetCounterForTesting(0), 0u);

  // Verify counters for the concurrent additions (none of which were evicted):
  EXPECT_EQ(filter.GetCounterForTesting(8), 256u);
  EXPECT_EQ(filter.GetCounterForTesting(16), 256u);
  EXPECT_EQ(filter.GetCounterForTesting(17), 128u);
  EXPECT_EQ(filter.GetCounterForTesting(32), 128u);
  EXPECT_EQ(filter.GetCounterForTesting(4), 128u);
  EXPECT_EQ(filter.GetCounterForTesting(2), 128u);

  // All concurrently added keys must be present.
  EXPECT_TRUE(filter.Contains(kAlfa));
  EXPECT_TRUE(filter.Contains(kBravo));
  EXPECT_TRUE(filter.Contains(kCharlie));
  EXPECT_TRUE(filter.Contains(kDelta));

  // Non-added keys must not be present.
  EXPECT_FALSE(filter.Contains(0));
}

}  // namespace
}  // namespace gwp_asan::internal
