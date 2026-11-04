// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/storage_durability.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_durability {

namespace {

constexpr char kQueriedHistogram[] = "Windows.StorageDurability.Queried";

// Recorded only once the drive has answered, so they stand or fall together.
constexpr const char* kFollowUpHistograms[] = {
    "Windows.StorageDurability.FlushCacheSupported",
    "Windows.StorageDurability.FlushingUnsafe",
    "Windows.StorageDurability.NVCacheEnabled",
    "Windows.StorageDurability.UserDefinedPowerProtection",
    "Windows.StorageDurability.WriteCacheEnabled",
    "Windows.StorageDurability.WriteCacheStateUnknown",
};

// The state a healthy consumer drive reports with both Device Manager
// checkboxes left at their defaults.
constexpr internal::StorageDurability kHealthyDrive = {
    .queried = true,
    .write_cache_enabled = true,
    .write_cache_state_unknown = false,
    .user_defined_power_protection = false,
    .nv_cache_enabled = false,
    .flush_cache_supported = true,
};

}  // namespace

TEST(IsWriteCacheFlushingUnsafeTest, CachedAndUnflushedIsUnsafe) {
  internal::StorageDurability durability = kHealthyDrive;
  durability.user_defined_power_protection = true;

  EXPECT_TRUE(internal::IsWriteCacheFlushingUnsafe(durability));
}

// A battery-backed cache is the configuration the Device Manager setting is
// actually meant for, so it clears the risk.
TEST(IsWriteCacheFlushingUnsafeTest, BatteryBackedCacheIsSafe) {
  internal::StorageDurability durability = kHealthyDrive;
  durability.user_defined_power_protection = true;
  durability.nv_cache_enabled = true;

  EXPECT_FALSE(internal::IsWriteCacheFlushingUnsafe(durability));
}

// Without an answer from the drive there is nothing to accuse it of, even when
// the rest of the struct holds values that would otherwise look alarming.
TEST(IsWriteCacheFlushingUnsafeTest, QueryFailedIsNotReportedAsUnsafe) {
  internal::StorageDurability durability = kHealthyDrive;
  durability.queried = false;
  durability.user_defined_power_protection = true;

  EXPECT_FALSE(internal::IsWriteCacheFlushingUnsafe(durability));
}

TEST(IsWriteCacheFlushingUnsafeTest, EitherHalfAloneIsSafe) {
  EXPECT_FALSE(internal::IsWriteCacheFlushingUnsafe(kHealthyDrive));

  internal::StorageDurability protection_only = kHealthyDrive;
  protection_only.write_cache_enabled = false;
  protection_only.user_defined_power_protection = true;
  EXPECT_FALSE(internal::IsWriteCacheFlushingUnsafe(protection_only));
}

// A drive that cannot flush on request is a separate concern from the Device
// Manager setting and does not on its own make flushing unsafe.
TEST(IsWriteCacheFlushingUnsafeTest, FlushNotSupportedAloneIsSafe) {
  internal::StorageDurability durability = kHealthyDrive;
  durability.flush_cache_supported = false;

  EXPECT_FALSE(internal::IsWriteCacheFlushingUnsafe(durability));
}

// A driver refusing to say whether the cache is on is not evidence that it is
// on, so it cannot make the drive unsafe by itself.
TEST(IsWriteCacheFlushingUnsafeTest, UnknownCacheStateIsNotTreatedAsCaching) {
  internal::StorageDurability durability = kHealthyDrive;
  durability.write_cache_enabled = false;
  durability.write_cache_state_unknown = true;
  durability.user_defined_power_protection = true;

  EXPECT_FALSE(internal::IsWriteCacheFlushingUnsafe(durability));
}

// The query names a volume as `\\.\X:`, so anything without a drive letter is
// rejected before any IOCTL is attempted. `flush_cache_supported` staying at
// its `true` default matters: it is what keeps a rejected path from being
// reported as a drive that cannot flush.
TEST(QueryStorageDurabilityTest, PathsWithoutADriveLetterAreRejected) {
  static constexpr const wchar_t* kPaths[] = {
      L"",
      L"\\\\server\\share\\profile",
      L"relative\\path",
      L"\\\\?\\C:\\profile",
  };

  for (const wchar_t* path : kPaths) {
    const internal::StorageDurability durability =
        internal::QueryStorageDurability(base::FilePath(path));

    EXPECT_FALSE(durability.queried) << path;
    EXPECT_FALSE(durability.write_cache_enabled) << path;
    EXPECT_FALSE(durability.write_cache_state_unknown) << path;
    EXPECT_FALSE(durability.user_defined_power_protection) << path;
    EXPECT_FALSE(durability.nv_cache_enabled) << path;
    EXPECT_TRUE(durability.flush_cache_supported) << path;
    EXPECT_FALSE(internal::IsWriteCacheFlushingUnsafe(durability)) << path;
  }
}

// Exercises the real IOCTL against whatever drive the bot happens to have.
// Nothing can be asserted about the hardware, only that the answer does not
// contradict itself.
TEST(QueryStorageDurabilityTest, RealDriveAnswerIsSelfConsistent) {
  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));

  const internal::StorageDurability durability =
      internal::QueryStorageDurability(temp_dir);

  // Both fields come from one tri-state value, so the drive cannot be caching
  // and unable to say at the same time.
  EXPECT_FALSE(durability.write_cache_enabled &&
               durability.write_cache_state_unknown);

  if (!durability.queried) {
    EXPECT_FALSE(durability.write_cache_enabled);
    EXPECT_FALSE(internal::IsWriteCacheFlushingUnsafe(durability));
  }
}

TEST(RecordStorageDurabilityMetricsTest, RecordsNothingWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kStorageDurabilityCheck);
  base::HistogramTester histogram_tester;

  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  RecordStorageDurabilityMetrics(temp_dir);

  EXPECT_TRUE(
      histogram_tester.GetTotalCountsForPrefix("Windows.StorageDurability.")
          .empty());
}

// A path with no drive letter is the one answer the test can arrange on any
// machine, and it must not drag the follow-ups along with it.
TEST(RecordStorageDurabilityMetricsTest,
     RecordsOnlyQueriedWhenDriveCannotBeAsked) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStorageDurabilityCheck);
  base::HistogramTester histogram_tester;

  RecordStorageDurabilityMetrics(base::FilePath(L"\\\\server\\share\\profile"));

  histogram_tester.ExpectUniqueSample(kQueriedHistogram, false, 1);
  for (const char* name : kFollowUpHistograms) {
    histogram_tester.ExpectTotalCount(name, 0);
  }
}

// Whether the bot's drive answers is not something the test can control, so
// pin the invariant instead: the follow-ups are all recorded or none are, and
// never more than once.
TEST(RecordStorageDurabilityMetricsTest, FollowUpHistogramsAreAllOrNothing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStorageDurabilityCheck);
  base::HistogramTester histogram_tester;

  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  RecordStorageDurabilityMetrics(temp_dir);

  histogram_tester.ExpectTotalCount(kQueriedHistogram, 1);

  const base::HistogramBase::Count32 expected_count =
      histogram_tester.GetBucketCount(kQueriedHistogram, true);
  for (const char* name : kFollowUpHistograms) {
    histogram_tester.ExpectTotalCount(name, expected_count);
  }
}

}  // namespace storage_durability
