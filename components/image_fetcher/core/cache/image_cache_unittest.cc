// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cache/image_cache.h"

#include <map>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/image_fetcher/core/cache/cached_image_fetcher_metrics_reporter.h"
#include "components/image_fetcher/core/cache/image_data_store_disk.h"
#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using testing::Mock;

namespace image_fetcher {

namespace {

constexpr char kPrefLastStartupEviction[] =
    "cached_image_fetcher_last_startup_eviction_time";
constexpr char kCachedImageFetcherEventHistogramName[] =
    "CachedImageFetcher.Events";
constexpr char kImageUrl[] = "http://gstatic.img.com/foo.jpg";
constexpr char kImageUrlHashed[] = "3H7UODDH3WKDWK6FQ3IZT3LQMVBPYJ4M";
constexpr char kImageData[] = "data";

}  // namespace

class ImageCacheTest : public testing::Test {
 public:
  ImageCacheTest() {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void CreateImageCache() {
    clock_.SetNow(base::Time());

    auto db = std::make_unique<FakeDB<CachedImageMetadataProto>>(&db_store_);
    db_ = db.get();
    auto metadata_store = std::make_unique<ImageMetadataStoreLevelDB>(
        base::FilePath(), std::move(db), &clock_);
    metadata_store_ = metadata_store.get();

    auto data_store = std::make_unique<ImageDataStoreDisk>(
        temp_dir_.GetPath(), base::SequencedTaskRunnerHandle::Get());
    data_store_ = data_store.get();

    ImageCache::RegisterProfilePrefs(test_prefs_.registry());
    image_cache_ = base::MakeRefCounted<ImageCache>(
        std::move(data_store), std::move(metadata_store), &test_prefs_, &clock_,
        base::SequencedTaskRunnerHandle::Get());
  }

  void InitializeImageCache() {
    image_cache_->MaybeStartInitialization();
    db()->InitCallback(true);
    RunUntilIdle();
    ASSERT_TRUE(metadata_store()->IsInitialized());
  }

  void PrepareImageCache() {
    CreateImageCache();
    InitializeImageCache();

    image_cache()->SaveImage(kImageUrl, kImageData);
    RunUntilIdle();

    ASSERT_TRUE(IsMetadataPresent(kImageUrlHashed));
  }

  bool IsCacheInitialized() {
    return image_cache()->AreAllDependenciesInitialized();
  }

  void RunEvictionOnStartup(bool success) {
    image_cache()->RunEvictionOnStartup();

    if (success) {
      db()->LoadCallback(true);
      db()->UpdateCallback(true);
      db()->LoadKeysCallback(true);
    }

    RunUntilIdle();
  }

  bool IsMetadataPresent(const std::string& key) {
    return db_store_.find(key) != db_store_.end();
  }

  CachedImageMetadataProto GetMetadata(const std::string& key) {
    if (!IsMetadataPresent(key)) {
      return CachedImageMetadataProto();
    }

    return CachedImageMetadataProto(db_store_[key]);
  }

  bool IsMetadataEqual(const CachedImageMetadataProto lhs,
                       const CachedImageMetadataProto rhs) {
    std::string lhs_str;
    lhs.SerializeToString(&lhs_str);
    std::string rhs_str;
    rhs.SerializeToString(&rhs_str);

    return lhs_str == rhs_str;
  }

  void RunReconciliation() {
    image_cache()->RunReconciliation();
    db()->LoadKeysCallback(true);
    RunUntilIdle();
    db()->UpdateCallback(true);
    RunUntilIdle();
  }

  void InjectMetadata(std::string key, int data_size) {
    metadata_store_->SaveImageMetadata(key, data_size);
  }

  void InjectData(std::string key, std::string data) {
    data_store_->SaveImage(key, data);
    RunUntilIdle();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  TestingPrefServiceSimple* prefs() { return &test_prefs_; }
  base::SimpleTestClock* clock() { return &clock_; }
  ImageCache* image_cache() { return image_cache_.get(); }
  ImageDataStoreDisk* data_store() { return data_store_; }
  ImageMetadataStoreLevelDB* metadata_store() { return metadata_store_; }
  FakeDB<CachedImageMetadataProto>* db() { return db_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  MOCK_METHOD1(DataCallback, void(std::string));

 private:
  scoped_refptr<ImageCache> image_cache_;
  ImageMetadataStoreLevelDB* metadata_store_;
  ImageDataStoreDisk* data_store_;
  base::SimpleTestClock clock_;

  TestingPrefServiceSimple test_prefs_;
  base::ScopedTempDir temp_dir_;
  FakeDB<CachedImageMetadataProto>* db_;
  std::map<std::string, CachedImageMetadataProto> db_store_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(ImageCacheTest);
};

TEST_F(ImageCacheTest, SanityTest) {
  CreateImageCache();
  InitializeImageCache();

  image_cache()->SaveImage(kImageUrl, kImageData);
  RunUntilIdle();

  EXPECT_CALL(*this, DataCallback(kImageData));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();

  image_cache()->DeleteImage(kImageUrl);
  RunUntilIdle();

  EXPECT_CALL(*this, DataCallback(std::string()));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(ImageCacheTest, SaveCallsInitialization) {
  CreateImageCache();

  ASSERT_FALSE(IsCacheInitialized());
  image_cache()->SaveImage(kImageUrl, kImageData);
  db()->InitCallback(true);
  RunUntilIdle();

  ASSERT_TRUE(IsCacheInitialized());
}

TEST_F(ImageCacheTest, Save) {
  CreateImageCache();
  InitializeImageCache();

  image_cache()->SaveImage(kImageUrl, kImageData);
  RunUntilIdle();

  EXPECT_CALL(*this, DataCallback(kImageData));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(ImageCacheTest, Load) {
  PrepareImageCache();
  auto metadata_before = GetMetadata(kImageUrlHashed);

  clock()->SetNow(clock()->Now() + base::TimeDelta::FromHours(1));
  EXPECT_CALL(*this, DataCallback(kImageData));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();
  db()->LoadCallback(true);
  db()->UpdateCallback(true);
  RunUntilIdle();

  auto metadata_after = GetMetadata(kImageUrlHashed);
  ASSERT_FALSE(IsMetadataEqual(metadata_before, metadata_after));
}

TEST_F(ImageCacheTest, LoadReadOnly) {
  PrepareImageCache();
  auto metadata_before = GetMetadata(kImageUrlHashed);

  clock()->SetNow(clock()->Now() + base::TimeDelta::FromHours(1));
  EXPECT_CALL(*this, DataCallback(kImageData));
  image_cache()->LoadImage(
      true, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();

  auto metadata_after = GetMetadata(kImageUrlHashed);
  ASSERT_TRUE(IsMetadataEqual(metadata_before, metadata_after));
}

TEST_F(ImageCacheTest, Delete) {
  PrepareImageCache();

  EXPECT_CALL(*this, DataCallback(kImageData));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();

  image_cache()->DeleteImage(kImageUrl);
  RunUntilIdle();

  EXPECT_CALL(*this, DataCallback(std::string()));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(ImageCacheTest, Eviction) {
  PrepareImageCache();

  clock()->SetNow(clock()->Now() + base::TimeDelta::FromDays(7));
  RunEvictionOnStartup(/* success */ true);
  ASSERT_EQ(clock()->Now(), prefs()->GetTime(kPrefLastStartupEviction));

  EXPECT_CALL(*this, DataCallback(std::string()));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();

  histogram_tester().ExpectBucketCount(
      kCachedImageFetcherEventHistogramName,
      CachedImageFetcherEvent::kCacheStartupEvictionStarted, 1);
  histogram_tester().ExpectBucketCount(
      kCachedImageFetcherEventHistogramName,
      CachedImageFetcherEvent::kCacheStartupEvictionFinished, 1);
}

TEST_F(ImageCacheTest, EvictionTooSoon) {
  PrepareImageCache();

  clock()->SetNow(clock()->Now() + base::TimeDelta::FromDays(6));
  RunEvictionOnStartup(/* success */ true);

  EXPECT_CALL(*this, DataCallback(kImageData));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(ImageCacheTest, EvictionWhenEvictionAlreadyPerformed) {
  PrepareImageCache();

  prefs()->SetTime("cached_image_fetcher_last_startup_eviction_time",
                   clock()->Now());
  clock()->SetNow(clock()->Now() + base::TimeDelta::FromHours(23));
  RunEvictionOnStartup(/* success */ false);

  EXPECT_CALL(*this, DataCallback(kImageData));
  image_cache()->LoadImage(
      false, kImageUrl,
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(ImageCacheTest, Reconciliation) {
  CreateImageCache();
  InitializeImageCache();

  // Inject differing keys so they mismatch, then run reconciliation.
  InjectData("foo", "z");
  InjectMetadata("bar", 10);
  RunReconciliation();

  // Data should be gone.
  EXPECT_CALL(*this, DataCallback(std::string()));
  image_cache()->LoadImage(
      false, "foo",
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();

  // Metadata should be gone.
  ASSERT_FALSE(IsMetadataPresent("bar"));
}

TEST_F(ImageCacheTest, ReconciliationMismatchData) {
  CreateImageCache();
  InitializeImageCache();

  // Inject differing keys so they mismatch, then run reconciliation.
  InjectData("foo", "z");
  InjectData("bar", "z");
  InjectMetadata("foo", 10);
  RunReconciliation();

  // Data should be gone.
  EXPECT_CALL(*this, DataCallback(std::string()));
  image_cache()->LoadImage(
      false, "bar",
      base::BindOnce(&ImageCacheTest::DataCallback, base::Unretained(this)));
  RunUntilIdle();
}

TEST_F(ImageCacheTest, ReconciliationMismatchMetadata) {
  CreateImageCache();
  InitializeImageCache();

  // Inject differing keys so they mismatch, then run reconciliation.
  InjectData("foo", "z");
  InjectMetadata("foo", 10);
  InjectMetadata("bar", 10);
  RunReconciliation();

  // Metadata should be gone.
  ASSERT_FALSE(IsMetadataPresent("bar"));
}

}  // namespace image_fetcher
