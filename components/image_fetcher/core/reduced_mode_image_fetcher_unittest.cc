// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/reduced_mode_image_fetcher.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cache/image_data_store_disk.h"
#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"
#include "components/image_fetcher/core/cached_image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/prefs/testing_pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using leveldb_proto::test::FakeDB;
using testing::_;

namespace image_fetcher {

namespace {

const GURL kImageUrl = GURL("http://gstatic.img.com/foo.jpg");

constexpr char kUmaClientName[] = "TestUma";
constexpr char kImageData[] = "data";

const char kImageFetcherEventHistogramName[] = "ImageFetcher.Events";

}  // namespace

class ReducedModeImageFetcherTest : public testing::Test {
 public:
  ReducedModeImageFetcherTest() {}

  ~ReducedModeImageFetcherTest() override {
    reduced_mode_image_fetcher_.reset();
    // We need to run until idle after deleting the database, because
    // ProtoDatabase deletes the actual LevelDB asynchronously.
    RunUntilIdle();
  }

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    ImageCache::RegisterProfilePrefs(test_prefs_.registry());
    CreateReducedModeImageFetcher();
  }

  void CreateReducedModeImageFetcher() {
    auto db =
        std::make_unique<FakeDB<CachedImageMetadataProto>>(&metadata_store_);
    db_ = db.get();

    auto metadata_store =
        std::make_unique<ImageMetadataStoreLevelDB>(std::move(db), &clock_);
    auto data_store = std::make_unique<ImageDataStoreDisk>(
        data_dir_.GetPath(), base::SequencedTaskRunnerHandle::Get());

    image_cache_ = base::MakeRefCounted<ImageCache>(
        std::move(data_store), std::move(metadata_store), &test_prefs_, &clock_,
        base::SequencedTaskRunnerHandle::Get());

    // Use an initial request to start the cache up.
    image_cache_->SaveImage(kImageUrl.spec(), kImageData,
                            /* needs_transcoding */ false);
    RunUntilIdle();
    db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    image_cache_->DeleteImage(kImageUrl.spec());
    RunUntilIdle();

    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
        /* ImageDecoder */ nullptr, shared_factory_);
    cached_image_fetcher_ = std::make_unique<CachedImageFetcher>(
        image_fetcher_.get(), image_cache_, /* read_only */ false);
    reduced_mode_image_fetcher_ =
        std::make_unique<ReducedModeImageFetcher>(cached_image_fetcher_.get());

    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void VerifyCacheHit() {
    RunUntilIdle();

    base::MockCallback<ImageDataFetcherCallback> data_callback;

    EXPECT_CALL(data_callback, Run(kImageData, _));
    reduced_mode_image_fetcher()->FetchImageAndData(
        kImageUrl, data_callback.Get(), ImageFetcherCallback(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
    db()->LoadCallback(true);
    RunUntilIdle();

    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kImageRequest, 1);
    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kCacheHit, 1);
  }

  ReducedModeImageFetcher* reduced_mode_image_fetcher() {
    return reduced_mode_image_fetcher_.get();
  }
  scoped_refptr<ImageCache> image_cache() { return image_cache_; }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  FakeDB<CachedImageMetadataProto>* db() { return db_; }

  MOCK_METHOD2(OnImageLoaded, void(bool, std::string));

 private:
  std::unique_ptr<ImageFetcher> image_fetcher_;
  std::unique_ptr<ImageFetcher> cached_image_fetcher_;
  std::unique_ptr<ReducedModeImageFetcher> reduced_mode_image_fetcher_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  scoped_refptr<ImageCache> image_cache_;
  base::SimpleTestClock clock_;
  TestingPrefServiceSimple test_prefs_;
  base::ScopedTempDir data_dir_;
  FakeDB<CachedImageMetadataProto>* db_;
  std::map<std::string, CachedImageMetadataProto> metadata_store_;

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(ReducedModeImageFetcherTest);
};

TEST_F(ReducedModeImageFetcherTest, FetchNeedsTranscodingImageFromCache) {
  // Save the image that needs transcoding in the database.
  image_cache()->SaveImage(kImageUrl.spec(), kImageData,
                           /* needs_transcoding */ true);
  VerifyCacheHit();
}

TEST_F(ReducedModeImageFetcherTest, FetchImageFromCache) {
  // Save the image that doesn't need transcoding in the database.
  image_cache()->SaveImage(kImageUrl.spec(), kImageData,
                           /* needs_transcoding */ false);
  VerifyCacheHit();
}

}  // namespace image_fetcher
