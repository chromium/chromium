// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_stats_tracker.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

constexpr char kPerThumbnailMemoryUsageHistogram[] =
    "Tab.Preview.MemoryUsage.CompressedData.PerThumbnailKiB";
constexpr char kTotalMemoryUsageHistogram[] =
    "Tab.Preview.MemoryUsage.CompressedData.TotalKiB";

class StubThumbnailImageDelegate : public ThumbnailImage::Delegate {
 public:
  StubThumbnailImageDelegate() = default;
  ~StubThumbnailImageDelegate() override = default;

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {}
};

class ThumbnailOwner : public ThumbnailImage::Delegate {
 public:
  ThumbnailOwner() = default;
  ~ThumbnailOwner() override = default;

  ThumbnailImage* Get() { return thumbnail_.get(); }

  // ThumbnailImage::Delegate:
  void ThumbnailImageBeingObservedChanged(bool is_being_observed) override {}

 private:
  scoped_refptr<ThumbnailImage> thumbnail_{
      base::MakeRefCounted<ThumbnailImage>(this)};
};

}  // namespace

class ThumbnailStatsTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Delete previous instance and create new one to start the timer.
    ThumbnailStatsTracker::ResetInstanceForTesting();
    ThumbnailStatsTracker::GetInstance();
  }

  // A random bitmap will have roughly the same size, or even greater
  // size, after compression.
  SkBitmap CreateRandomBitmapOfSize(unsigned int width, unsigned int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    for (unsigned int x = 0; x < width; ++x) {
      for (unsigned int y = 0; y < height; ++y) {
        *bitmap.getAddr32(x, y) = static_cast<uint32_t>(base::RandUint64());
      }
    }
    return bitmap;
  }

  void AssignThumbnailBitmapAndWait(ThumbnailImage* thumbnail,
                                    SkBitmap bitmap) {
    base::RunLoop run_loop;
    thumbnail->set_async_operation_finished_callback_for_testing(
        run_loop.QuitClosure());
    thumbnail->AssignSkBitmap(bitmap);
    run_loop.Run();

    // Clear the callback since the old one will be invalid.
    thumbnail->set_async_operation_finished_callback_for_testing(
        base::RepeatingClosure());
  }

  void AdvanceToNextReport() {
    task_environment_.FastForwardBy(ThumbnailStatsTracker::kReportingInterval);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::HistogramTester histogram_tester_;

 private:
  StubThumbnailImageDelegate stub_thumbnail_image_delegate_;
};

TEST_F(ThumbnailStatsTrackerTest, LogsMemoryMetricsAtHeartbeat) {
  ThumbnailOwner thumbnail_1;
  ThumbnailOwner thumbnail_2;

  AssignThumbnailBitmapAndWait(thumbnail_1.Get(),
                               CreateRandomBitmapOfSize(2, 2));
  AssignThumbnailBitmapAndWait(thumbnail_2.Get(),
                               CreateRandomBitmapOfSize(2, 2));

  histogram_tester_.ExpectTotalCount(kPerThumbnailMemoryUsageHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTotalMemoryUsageHistogram, 0);

  AdvanceToNextReport();
  histogram_tester_.ExpectTotalCount(kPerThumbnailMemoryUsageHistogram, 2);
  histogram_tester_.ExpectTotalCount(kTotalMemoryUsageHistogram, 1);

  AdvanceToNextReport();
  histogram_tester_.ExpectTotalCount(kPerThumbnailMemoryUsageHistogram, 4);
  histogram_tester_.ExpectTotalCount(kTotalMemoryUsageHistogram, 2);
}

TEST_F(ThumbnailStatsTrackerTest, AlwaysLogsTotal) {
  histogram_tester_.ExpectTotalCount(kPerThumbnailMemoryUsageHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kTotalMemoryUsageHistogram, 0, 0);

  AdvanceToNextReport();
  histogram_tester_.ExpectTotalCount(kPerThumbnailMemoryUsageHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kTotalMemoryUsageHistogram, 0, 1);

  AdvanceToNextReport();
  histogram_tester_.ExpectTotalCount(kPerThumbnailMemoryUsageHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kTotalMemoryUsageHistogram, 0, 2);
}

TEST_F(ThumbnailStatsTrackerTest, RecordedMemoryUsageIsCorrect) {
  ThumbnailOwner thumbnail_1;
  AssignThumbnailBitmapAndWait(thumbnail_1.Get(),
                               CreateRandomBitmapOfSize(200, 150));
  size_t thumbnail_1_size_kb =
      thumbnail_1.Get()->GetCompressedDataSizeInBytes() / 1024;

  AdvanceToNextReport();
  histogram_tester_.ExpectBucketCount(kPerThumbnailMemoryUsageHistogram,
                                      thumbnail_1_size_kb, 1);
  histogram_tester_.ExpectBucketCount(kTotalMemoryUsageHistogram,
                                      thumbnail_1_size_kb, 1);

  ThumbnailOwner thumbnail_2;
  AssignThumbnailBitmapAndWait(thumbnail_2.Get(),
                               CreateRandomBitmapOfSize(100, 100));
  size_t thumbnail_2_size_kb =
      thumbnail_2.Get()->GetCompressedDataSizeInBytes() / 1024;

  // This test won't work if the sizes are the same. While it's possible
  // that the two randomly generated bitmaps will compress to the same
  // size, the odds are astronomically low.
  ASSERT_NE(thumbnail_1_size_kb, thumbnail_2_size_kb);

  AdvanceToNextReport();
  histogram_tester_.ExpectBucketCount(kPerThumbnailMemoryUsageHistogram,
                                      thumbnail_1_size_kb, 2);
  histogram_tester_.ExpectBucketCount(kPerThumbnailMemoryUsageHistogram,
                                      thumbnail_2_size_kb, 1);
  histogram_tester_.ExpectBucketCount(kTotalMemoryUsageHistogram,
                                      thumbnail_1_size_kb, 1);
  histogram_tester_.ExpectBucketCount(
      kTotalMemoryUsageHistogram, thumbnail_1_size_kb + thumbnail_2_size_kb, 1);
}

TEST_F(ThumbnailStatsTrackerTest, EmptyThumbnailHasZeroSize) {
  ThumbnailOwner thumbnail;
  AdvanceToNextReport();
  histogram_tester_.ExpectUniqueSample(kPerThumbnailMemoryUsageHistogram, 0, 1);
  histogram_tester_.ExpectUniqueSample(kTotalMemoryUsageHistogram, 0, 1);
}
