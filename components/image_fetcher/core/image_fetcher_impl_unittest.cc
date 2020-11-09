// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;
using testing::Eq;
using testing::Property;

namespace image_fetcher {

namespace {

constexpr char kImageData[] = "data";
constexpr char kImageURL[] = "http://image.test/test.png";

constexpr char kImageUmaName[] = "TestUma";

class ImageFetcherImplTest : public testing::Test {
 public:
  ImageFetcherImplTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    auto decoder = std::make_unique<FakeImageDecoder>();
    fake_image_decoder_ = decoder.get();
    image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
        std::move(decoder), shared_factory_);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  FakeImageDecoder* image_decoder() { return fake_image_decoder_; }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  ImageFetcherImpl* image_fetcher() { return image_fetcher_.get(); }

 private:
  std::unique_ptr<ImageFetcherImpl> image_fetcher_;
  FakeImageDecoder* fake_image_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherImplTest);
};
MATCHER(ValidImage, "") {
  return arg.Width() == 2 && arg.Height() == 3;
}
MATCHER(EmptyImage, "") {
  return arg.Width() == 0;
}

TEST_F(ImageFetcherImplTest, FetchImageAndDataSuccess) {
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);
  base::MockCallback<ImageDataFetcherCallback> data_callback;
  base::MockCallback<ImageFetcherCallback> image_callback;
  EXPECT_CALL(data_callback, Run(kImageData, _));
  EXPECT_CALL(image_callback, Run(ValidImage(), _));

  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), data_callback.Get(), image_callback.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));
  RunUntilIdle();
}

TEST_F(ImageFetcherImplTest, FetchImageAndData3xSuccess) {
  // Fetch an image three times.
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);
  base::MockCallback<ImageDataFetcherCallback> data_callback1;
  base::MockCallback<ImageFetcherCallback> image_callback1;
  EXPECT_CALL(data_callback1, Run(kImageData, _));
  EXPECT_CALL(image_callback1, Run(ValidImage(), _));

  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), data_callback1.Get(), image_callback1.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));

  base::MockCallback<ImageDataFetcherCallback> data_callback2;
  base::MockCallback<ImageFetcherCallback> image_callback2;
  EXPECT_CALL(data_callback2, Run(kImageData, _));
  EXPECT_CALL(image_callback2, Run(ValidImage(), _));

  // This call happens before the network request completes.
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), data_callback2.Get(), image_callback2.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));

  base::MockCallback<ImageDataFetcherCallback> data_callback3;
  base::MockCallback<ImageFetcherCallback> image_callback3;
  EXPECT_CALL(data_callback3, Run(kImageData, _));
  EXPECT_CALL(image_callback3, Run(ValidImage(), _));

  image_decoder()->SetBeforeImageDecoded(base::BindLambdaForTesting([&]() {
    // This happens after the network request completes.
    // Shouldn't need to fetch.
    test_url_loader_factory()->AddResponse(kImageURL, "", net::HTTP_NOT_FOUND);
    image_fetcher()->FetchImageAndData(
        GURL(kImageURL), data_callback3.Get(), image_callback3.Get(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));
  }));

  RunUntilIdle();
}

TEST_F(ImageFetcherImplTest, FetchImageAndData2xFail) {
  // Fetch an image two times. The fetch fails.
  image_decoder()->SetEnabled(false);
  test_url_loader_factory()->AddResponse(kImageURL, "", net::HTTP_NOT_FOUND);
  base::MockCallback<ImageDataFetcherCallback> data_callback1;
  base::MockCallback<ImageFetcherCallback> image_callback1;
  EXPECT_CALL(data_callback1, Run("", _));
  EXPECT_CALL(image_callback1, Run(EmptyImage(), _));

  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), data_callback1.Get(), image_callback1.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));

  base::MockCallback<ImageDataFetcherCallback> data_callback2;
  base::MockCallback<ImageFetcherCallback> image_callback2;
  EXPECT_CALL(data_callback2, Run("", _));
  EXPECT_CALL(image_callback2, Run(EmptyImage(), _));

  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), data_callback2.Get(), image_callback2.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));

  RunUntilIdle();
}

TEST_F(ImageFetcherImplTest, FetchOnlyData) {
  image_decoder()->SetEnabled(false);
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);
  base::MockCallback<ImageDataFetcherCallback> data_callback;
  EXPECT_CALL(data_callback, Run(kImageData, _));

  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), data_callback.Get(), ImageFetcherCallback(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));

  RunUntilIdle();
}

TEST_F(ImageFetcherImplTest, FetchDataThenImage) {
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);
  base::MockCallback<ImageDataFetcherCallback> data_callback;
  EXPECT_CALL(data_callback, Run(kImageData, _));

  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), data_callback.Get(), ImageFetcherCallback(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));

  base::MockCallback<ImageFetcherCallback> image_callback;
  EXPECT_CALL(image_callback, Run(ValidImage(), _));

  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(), image_callback.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));

  RunUntilIdle();
}

TEST_F(ImageFetcherImplTest, FetchImageThenData) {
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);

  base::MockCallback<ImageFetcherCallback> image_callback;
  EXPECT_CALL(image_callback, Run(ValidImage(), _));

  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(), image_callback.Get(),
      ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));

  base::MockCallback<ImageDataFetcherCallback> data_callback;
  EXPECT_CALL(data_callback, Run(kImageData, _));

  image_decoder()->SetBeforeImageDecoded(base::BindLambdaForTesting([&]() {
    // This happens after the network request completes.
    // Shouldn't need to fetch.
    test_url_loader_factory()->AddResponse(kImageURL, "", net::HTTP_NOT_FOUND);
    image_fetcher()->FetchImageAndData(
        GURL(kImageURL), data_callback.Get(), ImageFetcherCallback(),
        ImageFetcherParams(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName));
  }));

  RunUntilIdle();
}

}  // namespace
}  // namespace image_fetcher
