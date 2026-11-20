// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_histogram_flattener.h"

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/record_histogram_checker.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/statistics_recorder.h"
#include "base/threading/platform_thread.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

// Ignore histograms emitted on background threads (e.g., Android runtime or
// periodic PartitionAlloc tasks in org.chromium.native_test) so they cannot
// pollute the temporary StatisticsRecorder during SetUp() or test assertions.
class TestThreadRecordHistogramChecker : public base::RecordHistogramChecker {
 public:
  bool ShouldRecord(uint32_t histogram_hash) const override {
    return base::PlatformThread::CurrentId() == test_thread_id_;
  }

 private:
  const base::PlatformThreadId test_thread_id_ =
      base::PlatformThread::CurrentId();
};

const cast::metrics::Histogram* LookupHistogram(
    std::string_view name,
    const std::vector<cast::metrics::Histogram>& deltas) {
  for (const auto& histogram : deltas) {
    if (histogram.name() == name) {
      return &histogram;
    }
  }
  return nullptr;
}

}  // namespace

class CastRuntimeHistogramFlattenerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Create the temporary stats recorder to get a clean state
    // for each test.
    statistics_recorder_ =
        base::StatisticsRecorder::CreateTemporaryForTesting();
    base::StatisticsRecorder::SetRecordChecker(
        std::make_unique<TestThreadRecordHistogramChecker>());

    // Drain any histogram recorded by a background thread before the checker
    // was installed, then verify the recorder starts clean.
    (void)GetHistogramDeltas();
    ASSERT_TRUE(GetHistogramDeltas().empty());
  }

  void TearDown() override {
    // Destroy the temporary stats recorder, implicitly restoring
    // the previous global stats recorder.
    statistics_recorder_.reset();
  }

  // Stand-in `StatisticsRecorder` for this test suite, to ensure
  // each test is isolated from everything else in the process that
  // could emit histograms.
  std::unique_ptr<base::StatisticsRecorder> statistics_recorder_;
};

TEST_F(CastRuntimeHistogramFlattenerTest, Empty) {
  EXPECT_TRUE(GetHistogramDeltas().empty());
}

TEST_F(CastRuntimeHistogramFlattenerTest, EmptyAfterPoll) {
  constexpr char kHistName[] = "Foo.Bar.SomeCount";
  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13);
  auto deltas = GetHistogramDeltas();
  EXPECT_EQ(1, GetCount(13, LookupHistogram(kHistName, deltas)));

  EXPECT_TRUE(GetHistogramDeltas().empty());
}

TEST_F(CastRuntimeHistogramFlattenerTest, Boolean) {
  constexpr char kBooleanName[] = "Foo.Bar.SomeBool";
  UMA_HISTOGRAM_BOOLEAN(kBooleanName, true);
  UMA_HISTOGRAM_BOOLEAN(kBooleanName, true);
  UMA_HISTOGRAM_BOOLEAN(kBooleanName, false);

  auto deltas = GetHistogramDeltas();
  EXPECT_EQ(GetCount(0, LookupHistogram(kBooleanName, deltas)), 1);
  EXPECT_EQ(GetCount(1, LookupHistogram(kBooleanName, deltas)), 2);
}

TEST_F(CastRuntimeHistogramFlattenerTest, Linear) {
  constexpr char kLinearName[] = "Foo.Bar.SomeLinear";
  constexpr int kHistogramLimit = 100;
  for (int i = 0; i < kHistogramLimit; ++i) {
    UMA_HISTOGRAM_EXACT_LINEAR(kLinearName, i, kHistogramLimit);
  }

  auto deltas = GetHistogramDeltas();

  const cast::metrics::Histogram* histogram =
      LookupHistogram(kLinearName, deltas);
  for (int i = 0; i < kHistogramLimit; ++i) {
    EXPECT_EQ(GetCount(i, histogram), 1);
  }
}

TEST_F(CastRuntimeHistogramFlattenerTest, Percentage) {
  constexpr char kPercentageName[] = "Foo.Bar.SomePercentage";
  for (int i = 0; i <= 100; ++i) {
    for (int j = 0; j < 2 * i; ++j) {
      UMA_HISTOGRAM_PERCENTAGE(kPercentageName, i);
    }
  }

  auto deltas = GetHistogramDeltas();

  const cast::metrics::Histogram* histogram =
      LookupHistogram(kPercentageName, deltas);
  for (int i = 0; i <= 100; ++i) {
    EXPECT_EQ(GetCount(i, histogram), 2 * i);
  }
}

TEST_F(CastRuntimeHistogramFlattenerTest, Enumeration) {
  constexpr char kEnumName[] = "Foo.Bar.SomeEnum";
  enum Category {
    kAlpha,
    kBeta,
    kGamma,
    kDelta,
    kCategoryMax = kDelta,
  };

  UMA_HISTOGRAM_ENUMERATION(kEnumName, kAlpha, kCategoryMax);
  UMA_HISTOGRAM_ENUMERATION(kEnumName, kGamma, kCategoryMax);
  UMA_HISTOGRAM_ENUMERATION(kEnumName, kDelta, kCategoryMax);
  UMA_HISTOGRAM_ENUMERATION(kEnumName, kGamma, kCategoryMax);

  auto deltas = GetHistogramDeltas();

  const cast::metrics::Histogram* histogram =
      LookupHistogram(kEnumName, deltas);
  EXPECT_EQ(1, GetCount(kAlpha, histogram));
  EXPECT_EQ(0, GetCount(kBeta, histogram));
  EXPECT_EQ(2, GetCount(kGamma, histogram));
  EXPECT_EQ(1, GetCount(kDelta, histogram));
}

TEST_F(CastRuntimeHistogramFlattenerTest, MultipleHistograms) {
  constexpr char kHistName1[] = "Foo.Bar.SomeCount";
  constexpr char kHistName2[] = "Foo.Bar.SomeOtherCount";

  UMA_HISTOGRAM_COUNTS_1M(kHistName1, 13);
  UMA_HISTOGRAM_COUNTS_1M(kHistName2, 1337);
  UMA_HISTOGRAM_COUNTS_1M(kHistName2, 1337);

  auto deltas = GetHistogramDeltas();
  EXPECT_EQ(1, GetCount(13, LookupHistogram(kHistName1, deltas)));
  EXPECT_EQ(2, GetCount(1337, LookupHistogram(kHistName2, deltas)));
}

TEST_F(CastRuntimeHistogramFlattenerTest, SeparateBuckets) {
  constexpr char kHistName[] = "Foo.Bar.SomeCount";

  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13);
  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13000);

  auto deltas = GetHistogramDeltas();
  EXPECT_EQ(1, GetCount(13, LookupHistogram(kHistName, deltas)));
  EXPECT_EQ(1, GetCount(13000, LookupHistogram(kHistName, deltas)));

  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13000);
  UMA_HISTOGRAM_COUNTS_1M(kHistName, 13000);
  deltas = GetHistogramDeltas();
  EXPECT_EQ(2, GetCount(13000, LookupHistogram(kHistName, deltas)));
}

class FakeSubprocessHistogramProvider
    : public base::StatisticsRecorder::HistogramProvider {
 public:
  FakeSubprocessHistogramProvider() {
    base::StatisticsRecorder::RegisterHistogramProvider(
        weak_factory_.GetWeakPtr());
  }

  void MergeHistogramDeltas(bool async,
                            base::OnceClosure done_callback) override {
    base::UmaHistogramBoolean("Subprocess.Video.SubVsyncCollision", true);
    std::move(done_callback).Run();
  }

 private:
  base::WeakPtrFactory<FakeSubprocessHistogramProvider> weak_factory_{this};
};

TEST_F(CastRuntimeHistogramFlattenerTest, ImportsProvidedSubprocessHistograms) {
  FakeSubprocessHistogramProvider provider;

  auto deltas = GetHistogramDeltas();
  const cast::metrics::Histogram* delta =
      LookupHistogram("Subprocess.Video.SubVsyncCollision", deltas);
  ASSERT_NE(nullptr, delta);
  EXPECT_EQ(1, GetCount(1, delta));
}

TEST_F(CastRuntimeHistogramFlattenerTest, ImportsPersistentHistograms) {
  base::GlobalHistogramAllocator* old_allocator =
      base::GlobalHistogramAllocator::ReleaseForTesting();
  base::ScopedClosureRunner restore_allocator(base::BindOnce(
      [](base::GlobalHistogramAllocator* previous) {
        base::GlobalHistogramAllocator::ReleaseForTesting();
        if (previous) {
          base::GlobalHistogramAllocator::Set(previous);
        }
      },
      old_allocator));

  base::GlobalHistogramAllocator::CreateWithLocalMemory(
      64 << 10, 0, "CastRuntimeHistogramFlattenerTest");
  ASSERT_NE(nullptr, base::GlobalHistogramAllocator::Get());
  base::PersistentSampleVector::
      ResetMountExistingCountsStorageResultForTesting();

  constexpr char kPersistentHistName[] =
      "Subprocess.Video.PersistentSubVsyncCollision";
  base::UmaHistogramBoolean(kPersistentHistName, true);
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(kPersistentHistName);
  ASSERT_NE(nullptr, histogram);
  EXPECT_TRUE(histogram->HasFlags(base::HistogramBase::kIsPersistent));

  auto deltas = GetHistogramDeltas();
  const cast::metrics::Histogram* delta =
      LookupHistogram(kPersistentHistName, deltas);
  ASSERT_NE(nullptr, delta);
  EXPECT_EQ(1, GetCount(1, delta));
}

}  // namespace chromecast
