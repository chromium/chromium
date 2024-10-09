// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/cached_image_fetcher.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/cache/image_data_store_disk.h"
#include "components/image_fetcher/core/cache/image_metadata_store_leveldb.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
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

using testing::_;
using leveldb_proto::test::FakeDB;

namespace image_fetcher {

class FakeImageDecoder;

namespace {

constexpr char kUmaClientName[] = "TestUma";
constexpr char kImageData[] = "data";
constexpr char kImageDataOther[] = "other";

const char kImageFetcherEventHistogramName[] = "ImageFetcher.Events";
const char kCacheLoadHistogramName[] = "ImageFetcher.ImageLoadFromCacheTime";
const char kNetworkLoadHistogramName[] =
    "ImageFetcher.ImageLoadFromNetworkTime";

}  // namespace

class CachedImageFetcherTest : public testing::Test {
 public:
  CachedImageFetcherTest() = default;

  CachedImageFetcherTest(const CachedImageFetcherTest&) = delete;
  CachedImageFetcherTest& operator=(const CachedImageFetcherTest&) = delete;

  ~CachedImageFetcherTest() override {
    cached_image_fetcher_.reset();
    // We need to run until idle after deleting the database, because
    // ProtoDatabase deletes the actual LevelDB asynchronously.
    RunUntilIdle();
  }

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    ImageCache::RegisterProfilePrefs(test_prefs_.registry());
    CreateCachedImageFetcher(false);
  }

  void CreateCachedImageFetcher(bool read_only) {
    auto db =
        std::make_unique<FakeDB<CachedImageMetadataProto>>(&metadata_store_);
    db_ = db.get();

    auto metadata_store =
        std::make_unique<ImageMetadataStoreLevelDB>(std::move(db), &clock_);
    auto data_store = std::make_unique<ImageDataStoreDisk>(
        data_dir_.GetPath(), base::SequencedTaskRunner::GetCurrentDefault());

    image_cache_ = base::MakeRefCounted<ImageCache>(
        std::move(data_store), std::move(metadata_store), &test_prefs_, &clock_,
        base::SequencedTaskRunner::GetCurrentDefault());

    // Use an initial request to start the cache up.
    const std::string kImageUrl("http://gstatic.img.com/foo.jpg");
    image_cache_->SaveImage(kImageUrl, kImageData,
                            /* needs_transcoding */ false,
                            /* expiration_interval */ std::nullopt);
    RunUntilIdle();
    db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    image_cache_->DeleteImage(kImageUrl);
    RunUntilIdle();

    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    auto decoder = std::make_unique<FakeImageDecoder>();
    fake_image_decoder_ = decoder.get();

    image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
        std::move(decoder), shared_factory_);
    cached_image_fetcher_ = std::make_unique<CachedImageFetcher>(
        image_fetcher_.get(), image_cache_, read_only);

    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  CachedImageFetcher* cached_image_fetcher() {
    return cached_image_fetcher_.get();
  }
  scoped_refptr<ImageCache> image_cache() { return image_cache_; }
  FakeImageDecoder* image_decoder() { return fake_image_decoder_; }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  FakeDB<CachedImageMetadataProto>* db() { return db_; }

  MOCK_METHOD2(OnImageLoaded, void(bool, std::string));

 private:
  std::unique_ptr<ImageFetcher> image_fetcher_;
  std::unique_ptr<CachedImageFetcher> cached_image_fetcher_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  raw_ptr<FakeImageDecoder> fake_image_decoder_;

  scoped_refptr<ImageCache> image_cache_;
  base::SimpleTestClock clock_;
  TestingPrefServiceSimple test_prefs_;
  base::ScopedTempDir data_dir_;
  raw_ptr<FakeDB<CachedImageMetadataProto>> db_;
  std::map<std::string, CachedImageMetadataProto> metadata_store_;

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

MATCHER(EmptyImage, "") {
  return arg.IsEmpty();
}

MATCHER(NonEmptyImage, "") {
  return !arg.IsEmpty();
}

MATCHER(NonEmptyString, "") {
  return !arg.empty();
}

// TODO(wylieb): Write a test that creates two CachedImageFetcher and tests
// that they both can use what's inside.
TEST_F(CachedImageFetcherTest, FetchImageFromCache) {
  // Save the image in the database.
  const GURL kImageUrl("http://gstatic.img.com/foo.jpg");
  image_cache()->SaveImage(kImageUrl.spec(), kImageData,
                           /* needs_transcoding */ false,
                           /* expiration_interval */ std::nullopt);
  RunUntilIdle();

  base::MockCallback<ImageDataFetcherCallback> data_callback;
  base::MockCallback<ImageFetcherCallback> image_callback;

  EXPECT_CALL(data_callback, Run(kImageData, _));
  EXPECT_CALL(image_callback, Run(NonEmptyImage(), _));
  cached_image_fetcher()->FetchImageAndData(
      kImageUrl, data_callback.Get(), image_callback.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
  db()->LoadCallback(true);
  RunUntilIdle();

  histogram_tester().ExpectTotalCount(kCacheLoadHistogramName, 1);
  histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                       ImageFetcherEvent::kImageRequest, 1);
  histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                       ImageFetcherEvent::kCacheHit, 1);
}

TEST_F(CachedImageFetcherTest, FetchImageFromCacheNeedsTranscoding) {
  // Save the image in the database.
  const GURL kImageUrl("http://gstatic.img.com/foo.jpg");
  image_cache()->SaveImage(kImageUrl.spec(), kImageData,
                           /* needs_transcoding */ true,
                           /* expiration_interval */ std::nullopt);
  RunUntilIdle();

  base::MockCallback<ImageDataFetcherCallback> data_callback;
  base::MockCallback<ImageFetcherCallback> image_callback;

  EXPECT_CALL(data_callback, Run(kImageData, _));
  EXPECT_CALL(image_callback, Run(NonEmptyImage(), _));
  cached_image_fetcher()->FetchImageAndData(
      kImageUrl, data_callback.Get(), image_callback.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
  db()->LoadCallback(true);
  RunUntilIdle();

  histogram_tester().ExpectBucketCount(
      kImageFetcherEventHistogramName,
      ImageFetcherEvent::kImageQueuedForTranscodingDecoded, 1);
  histogram_tester().ExpectBucketCount(
      kImageFetcherEventHistogramName,
      ImageFetcherEvent::kImageQueuedForTranscodingStoredBack, 1);
}

TEST_F(CachedImageFetcherTest, FetchImageFromCacheReadOnly) {
  CreateCachedImageFetcher(/* read_only */ true);
  // Save the image in the database.
  const GURL kImageUrl("http://gstatic.img.com/foo.jpg");
  image_cache()->SaveImage(kImageUrl.spec(), kImageData,
                           /* needs_transcoding */ false,
                           /* expiration_interval */ std::nullopt);
  test_url_loader_factory()->AddResponse(kImageUrl.spec(), kImageData);
  RunUntilIdle();
  {
    // Even if there's a decoding error, read_only cache shouldn't alter the
    // cache.
    image_decoder()->SetDecodingValid(false);
    base::MockCallback<ImageDataFetcherCallback> data_callback;
    base::MockCallback<ImageFetcherCallback> image_callback;
    EXPECT_CALL(image_callback, Run(EmptyImage(), _));
    cached_image_fetcher()->FetchImageAndData(
        kImageUrl, data_callback.Get(), image_callback.Get(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
    db()->LoadCallback(true);
    RunUntilIdle();

    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kImageRequest, 1);
    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kCacheHit, 1);
    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kCacheDecodingError,
                                         1);
  }
  {
    // Image should still be in the cache.
    image_decoder()->SetDecodingValid(true);
    base::MockCallback<ImageDataFetcherCallback> data_callback;
    base::MockCallback<ImageFetcherCallback> image_callback;
    EXPECT_CALL(image_callback, Run(NonEmptyImage(), _));
    cached_image_fetcher()->FetchImageAndData(
        kImageUrl, data_callback.Get(), image_callback.Get(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
    db()->LoadCallback(true);
    RunUntilIdle();
  }
}

TEST_F(CachedImageFetcherTest, FetchImagePopulatesCache) {
  // Expect the image to be fetched by URL.
  const GURL kImageUrl("http://gstatic.img.com/foo.jpg");
  {
    test_url_loader_factory()->AddResponse(kImageUrl.spec(), kImageData);

    base::MockCallback<ImageDataFetcherCallback> data_callback;
    base::MockCallback<ImageFetcherCallback> image_callback;

    EXPECT_CALL(data_callback, Run(NonEmptyString(), _));
    EXPECT_CALL(image_callback, Run(NonEmptyImage(), _));
    cached_image_fetcher()->FetchImageAndData(
        kImageUrl, data_callback.Get(), image_callback.Get(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
    db()->LoadCallback(true);
    RunUntilIdle();

    histogram_tester().ExpectTotalCount(kNetworkLoadHistogramName, 1);
    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kImageRequest, 1);
    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kCacheMiss, 1);
  }
  // Make sure the image data is in the database.
  {
    EXPECT_CALL(*this, OnImageLoaded(false, NonEmptyString()));
    image_cache()->LoadImage(
        /* read_only */ false, kImageUrl.spec(),
        base::BindOnce(&CachedImageFetcherTest::OnImageLoaded,
                       base::Unretained(this)));
    db()->LoadCallback(true);
    RunUntilIdle();
  }
  // Fetch again. The cache should be populated, no network request is needed.
  {
    test_url_loader_factory()->ClearResponses();

    base::MockCallback<ImageDataFetcherCallback> data_callback;
    base::MockCallback<ImageFetcherCallback> image_callback;

    EXPECT_CALL(data_callback, Run(NonEmptyString(), _));
    EXPECT_CALL(image_callback, Run(NonEmptyImage(), _));
    cached_image_fetcher()->FetchImageAndData(
        kImageUrl, data_callback.Get(), image_callback.Get(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
    db()->LoadCallback(true);
    RunUntilIdle();
  }
}

TEST_F(CachedImageFetcherTest, FetchImagePopulatesCacheReadOnly) {
  CreateCachedImageFetcher(/* read_only */ true);
  // Expect the image to be fetched by URL.
  const GURL kImageUrl("http://gstatic.img.com/foo.jpg");
  {
    test_url_loader_factory()->AddResponse(kImageUrl.spec(), kImageData);

    base::MockCallback<ImageDataFetcherCallback> data_callback;
    base::MockCallback<ImageFetcherCallback> image_callback;

    EXPECT_CALL(data_callback, Run(NonEmptyString(), _));
    EXPECT_CALL(image_callback, Run(NonEmptyImage(), _));
    cached_image_fetcher()->FetchImageAndData(
        kImageUrl, data_callback.Get(), image_callback.Get(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
    db()->LoadCallback(true);
    RunUntilIdle();

    histogram_tester().ExpectTotalCount(kNetworkLoadHistogramName, 1);
    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kImageRequest, 1);
    histogram_tester().ExpectBucketCount(kImageFetcherEventHistogramName,
                                         ImageFetcherEvent::kCacheMiss, 1);
  }
  // Make sure the image data is not in the database.
  {
    EXPECT_CALL(*this, OnImageLoaded(false, std::string()));
    image_cache()->LoadImage(
        /* read_only */ false, kImageUrl.spec(),
        base::BindOnce(&CachedImageFetcherTest::OnImageLoaded,
                       base::Unretained(this)));
    db()->LoadCallback(true);
    RunUntilIdle();
  }
}

TEST_F(CachedImageFetcherTest, FetchImageWithoutTranscodingDoesNotDecode) {
  const GURL kImageUrl("http://gstatic.img.com/foo.jpg");
  {
    test_url_loader_factory()->AddResponse(kImageUrl.spec(), kImageData);
    image_decoder()->SetDecodingValid(false);

    base::MockCallback<ImageDataFetcherCallback> data_callback;

    EXPECT_CALL(data_callback, Run(kImageData, _));
    ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName);
    params.set_skip_transcoding_for_testing(true);
    cached_image_fetcher()->FetchImageAndData(kImageUrl, data_callback.Get(),
                                              ImageFetcherCallback(), params);
    db()->LoadCallback(true);
    RunUntilIdle();
  }
  {
    test_url_loader_factory()->ClearResponses();
    base::MockCallback<ImageDataFetcherCallback> data_callback;
    EXPECT_CALL(data_callback, Run(kImageData, _));
    cached_image_fetcher()->FetchImageAndData(
        kImageUrl, data_callback.Get(), ImageFetcherCallback(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName));
    db()->LoadCallback(true);
    RunUntilIdle();
  }
}

TEST_F(CachedImageFetcherTest, FetchImageWithSkipDiskCache) {
  // Save the image in the database.
  const GURL kImageUrl("http://gstatic.img.com/foo.jpg");
  image_cache()->SaveImage(kImageUrl.spec(), kImageDataOther,
                           /* needs_transcoding */ false,
                           /* expiration_interval */ std::nullopt);
  RunUntilIdle();
  test_url_loader_factory()->AddResponse(kImageUrl.spec(), kImageData);

  base::MockCallback<ImageDataFetcherCallback> data_callback;
  base::MockCallback<ImageFetcherCallback> image_callback;

  ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS, kUmaClientName);
  params.set_skip_disk_cache_read(true);

  EXPECT_CALL(data_callback, Run(kImageData, _));
  EXPECT_CALL(image_callback, Run(NonEmptyImage(), _));
  cached_image_fetcher()->FetchImageAndData(kImageUrl, data_callback.Get(),
                                            image_callback.Get(), params);

  RunUntilIdle();
}

}  // namespace image_fetcher
