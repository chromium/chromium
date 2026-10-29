// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/mock_image_decoder.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
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

  ImageFetcherImplTest(const ImageFetcherImplTest&) = delete;
  ImageFetcherImplTest& operator=(const ImageFetcherImplTest&) = delete;

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  FakeImageDecoder* image_decoder() { return fake_image_decoder_; }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  ImageFetcherImpl* image_fetcher() { return image_fetcher_.get(); }

  void RecreateFetcherWithDecoder(std::unique_ptr<ImageDecoder> decoder) {
    // Clear the non-owning pointer before destroying its decoder owner.
    fake_image_decoder_ = nullptr;
    image_fetcher_ =
        std::make_unique<ImageFetcherImpl>(std::move(decoder), shared_factory_);
  }

  void DestroyImageFetcher() {
    fake_image_decoder_ = nullptr;
    image_fetcher_.reset();
  }

 private:
  std::unique_ptr<ImageFetcherImpl> image_fetcher_;
  raw_ptr<FakeImageDecoder> fake_image_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  base::test::TaskEnvironment task_environment_;
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

// The parameter selects whether the smaller frame/download limit is requested
// first. Both requests remain in flight together for the same URL.
class ImageFetcherRequestOptionsTest
    : public ImageFetcherImplTest,
      public testing::WithParamInterface<bool> {};

TEST_P(ImageFetcherRequestOptionsTest, ConcurrentRequestsHonorFrameSizes) {
  auto decoder = std::make_unique<MockImageDecoder>();
  // Model selection of an embedded ICO frame using the size passed to the
  // decoder, without assuming whether downloads or decoding are shared.
  EXPECT_CALL(*decoder, DecodeImage(_, _, _, _))
      .WillRepeatedly([](const std::string&, const gfx::Size& size,
                         data_decoder::DataDecoder*,
                         ImageDecodedCallback callback) {
        std::move(callback).Run(
            gfx::test::CreateImage(size.width(), size.height()));
      });
  RecreateFetcherWithDecoder(std::move(decoder));

  const GURL image_url("http://image.test/test.ico");
  const gfx::Size first_size =
      GetParam() ? gfx::Size(16, 16) : gfx::Size(64, 64);
  const gfx::Size second_size =
      GetParam() ? gfx::Size(64, 64) : gfx::Size(16, 16);
  ImageFetcherParams first_params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  first_params.set_frame_size(first_size);
  ImageFetcherParams second_params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  second_params.set_frame_size(second_size);
  base::test::TestFuture<gfx::Image, RequestMetadata> first;
  base::test::TestFuture<gfx::Image, RequestMetadata> second;
  image_fetcher()->FetchImageAndData(
      image_url, ImageDataFetcherCallback(),
      first.GetCallback<const gfx::Image&, const RequestMetadata&>(),
      first_params);
  image_fetcher()->FetchImageAndData(
      image_url, ImageDataFetcherCallback(),
      second.GetCallback<const gfx::Image&, const RequestMetadata&>(),
      second_params);
  test_url_loader_factory()->AddResponse(image_url.spec(), kImageData);

  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(second.Wait());
  EXPECT_EQ(first_size, first.Get<0>().Size());
  EXPECT_EQ(second_size, second.Get<0>().Size());
}

TEST_P(ImageFetcherRequestOptionsTest, ConcurrentRequestsHonorDownloadLimits) {
  const int64_t first_limit = GetParam() ? 64 : 256;
  const int64_t second_limit = GetParam() ? 256 : 64;
  const std::string response(128, 'x');
  ImageFetcherParams first_params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  first_params.set_max_download_size(first_limit);
  ImageFetcherParams second_params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  second_params.set_max_download_size(second_limit);
  base::test::TestFuture<std::string, RequestMetadata> first;
  base::test::TestFuture<std::string, RequestMetadata> second;
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL),
      first.GetCallback<const std::string&, const RequestMetadata&>(),
      ImageFetcherCallback(), first_params);
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL),
      second.GetCallback<const std::string&, const RequestMetadata&>(),
      ImageFetcherCallback(), second_params);
  test_url_loader_factory()->AddResponse(kImageURL, response);

  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(second.Wait());
  EXPECT_EQ(first_limit < 128 ? std::string() : response, first.Get<0>());
  EXPECT_EQ(first_limit < 128 ? net::ERR_INSUFFICIENT_RESOURCES : net::OK,
            first.Get<1>().net_error);
  EXPECT_EQ(second_limit < 128 ? std::string() : response, second.Get<0>());
  EXPECT_EQ(second_limit < 128 ? net::ERR_INSUFFICIENT_RESOURCES : net::OK,
            second.Get<1>().net_error);
}

TEST_P(ImageFetcherRequestOptionsTest, UnboundedAndLimitedRequestsAreSeparate) {
  const std::string response(128, 'x');
  ImageFetcherParams limited_params(TRAFFIC_ANNOTATION_FOR_TESTS,
                                    kImageUmaName);
  limited_params.set_max_download_size(64);
  ImageFetcherParams unbounded_params(TRAFFIC_ANNOTATION_FOR_TESTS,
                                      kImageUmaName);
  base::test::TestFuture<std::string, RequestMetadata> limited;
  base::test::TestFuture<std::string, RequestMetadata> unbounded;
  auto fetch_limited = [&] {
    image_fetcher()->FetchImageAndData(
        GURL(kImageURL),
        limited.GetCallback<const std::string&, const RequestMetadata&>(),
        ImageFetcherCallback(), limited_params);
  };
  auto fetch_unbounded = [&] {
    image_fetcher()->FetchImageAndData(
        GURL(kImageURL),
        unbounded.GetCallback<const std::string&, const RequestMetadata&>(),
        ImageFetcherCallback(), unbounded_params);
  };
  if (GetParam()) {
    fetch_limited();
    fetch_unbounded();
  } else {
    fetch_unbounded();
    fetch_limited();
  }
  EXPECT_EQ(2, test_url_loader_factory()->NumPending());
  test_url_loader_factory()->AddResponse(kImageURL, response);

  ASSERT_TRUE(limited.Wait());
  EXPECT_TRUE(limited.Get<0>().empty());
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES, limited.Get<1>().net_error);
  ASSERT_TRUE(unbounded.Wait());
  EXPECT_EQ(response, unbounded.Get<0>());
  EXPECT_EQ(net::OK, unbounded.Get<1>().net_error);
}

TEST_F(ImageFetcherImplTest, CompatibleRequestsShareDownload) {
  ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  params.set_max_download_size(256);
  params.set_frame_size(gfx::Size(16, 16));
  base::test::TestFuture<std::string, RequestMetadata> first;
  base::test::TestFuture<std::string, RequestMetadata> second;
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL),
      first.GetCallback<const std::string&, const RequestMetadata&>(),
      ImageFetcherCallback(), params);
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL),
      second.GetCallback<const std::string&, const RequestMetadata&>(),
      ImageFetcherCallback(), params);
  EXPECT_EQ(1, test_url_loader_factory()->NumPending());
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);

  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(second.Wait());
  EXPECT_EQ(kImageData, first.Get<0>());
  EXPECT_EQ(kImageData, second.Get<0>());
}

TEST_F(ImageFetcherImplTest, DifferentFrameSizeWhileDecodingStartsNewRequest) {
  ImageDecodedCallback pending_decode;
  base::test::TestFuture<void> decoding_started;
  auto decoder = std::make_unique<MockImageDecoder>();
  EXPECT_CALL(*decoder, DecodeImage(_, gfx::Size(16, 16), _, _))
      .WillOnce([&](const std::string&, const gfx::Size&,
                    data_decoder::DataDecoder*, ImageDecodedCallback callback) {
        pending_decode = std::move(callback);
        decoding_started.SetValue();
      });
  EXPECT_CALL(*decoder, DecodeImage(_, gfx::Size(64, 64), _, _))
      .WillOnce([](const std::string&, const gfx::Size&,
                   data_decoder::DataDecoder*, ImageDecodedCallback callback) {
        std::move(callback).Run(gfx::test::CreateImage(64, 64));
      });
  RecreateFetcherWithDecoder(std::move(decoder));

  ImageFetcherParams small_params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  small_params.set_frame_size(gfx::Size(16, 16));
  ImageFetcherParams large_params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  large_params.set_frame_size(gfx::Size(64, 64));
  base::test::TestFuture<gfx::Image, RequestMetadata> small;
  base::test::TestFuture<gfx::Image, RequestMetadata> large;
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(),
      small.GetCallback<const gfx::Image&, const RequestMetadata&>(),
      small_params);
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);
  ASSERT_TRUE(decoding_started.Wait());
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(),
      large.GetCallback<const gfx::Image&, const RequestMetadata&>(),
      large_params);
  std::move(pending_decode).Run(gfx::test::CreateImage(16, 16));

  ASSERT_TRUE(small.Wait());
  ASSERT_TRUE(large.Wait());
  EXPECT_EQ(gfx::Size(16, 16), small.Get<0>().Size());
  EXPECT_EQ(gfx::Size(64, 64), large.Get<0>().Size());
}

TEST_F(ImageFetcherImplTest, DataCallbackCanRequestSameImageAgain) {
  ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  base::test::TestFuture<std::string, RequestMetadata> first;
  base::test::TestFuture<std::string, RequestMetadata> reentrant;
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL),
      base::BindLambdaForTesting([&](const std::string& data,
                                     const RequestMetadata& metadata) {
        first.SetValue(data, metadata);
        image_fetcher()->FetchImageAndData(
            GURL(kImageURL),
            reentrant.GetCallback<const std::string&, const RequestMetadata&>(),
            ImageFetcherCallback(), params);
      }),
      ImageFetcherCallback(), params);
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);

  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(reentrant.Wait());
  EXPECT_EQ(kImageData, first.Get<0>());
  EXPECT_EQ(kImageData, reentrant.Get<0>());
}

TEST_F(ImageFetcherImplTest, DataCallbackCanRequestSameFailedImageAgain) {
  ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  base::test::TestFuture<std::string, RequestMetadata> first;
  base::test::TestFuture<std::string, RequestMetadata> reentrant;
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL),
      base::BindLambdaForTesting([&](const std::string& data,
                                     const RequestMetadata& metadata) {
        first.SetValue(data, metadata);
        image_fetcher()->FetchImageAndData(
            GURL(kImageURL),
            reentrant.GetCallback<const std::string&, const RequestMetadata&>(),
            ImageFetcherCallback(), params);
      }),
      ImageFetcherCallback(), params);
  test_url_loader_factory()->AddResponse(kImageURL, "", net::HTTP_NOT_FOUND);

  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(reentrant.Wait());
  EXPECT_TRUE(first.Get<0>().empty());
  EXPECT_TRUE(reentrant.Get<0>().empty());
  EXPECT_EQ(net::HTTP_NOT_FOUND, reentrant.Get<1>().http_response_code);
}

TEST_F(ImageFetcherImplTest, ImageCallbackCanRequestSameImageAgain) {
  ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  base::test::TestFuture<gfx::Image, RequestMetadata> first;
  base::test::TestFuture<gfx::Image, RequestMetadata> reentrant;
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(),
      base::BindLambdaForTesting([&](const gfx::Image& image,
                                     const RequestMetadata& metadata) {
        first.SetValue(image, metadata);
        image_fetcher()->FetchImageAndData(
            GURL(kImageURL), ImageDataFetcherCallback(),
            reentrant.GetCallback<const gfx::Image&, const RequestMetadata&>(),
            params);
      }),
      params);
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);

  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(reentrant.Wait());
  EXPECT_FALSE(first.Get<0>().IsEmpty());
  EXPECT_FALSE(reentrant.Get<0>().IsEmpty());
}

TEST_F(ImageFetcherImplTest, DataCallbackCanDestroyFetcher) {
  ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  base::test::TestFuture<void> destroyed;
  base::MockCallback<ImageDataFetcherCallback> cancelled;
  EXPECT_CALL(cancelled, Run(_, _)).Times(0);
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL),
      base::BindLambdaForTesting(
          [&](const std::string&, const RequestMetadata&) {
            DestroyImageFetcher();
            destroyed.SetValue();
          }),
      ImageFetcherCallback(), params);
  image_fetcher()->FetchImageAndData(GURL(kImageURL), cancelled.Get(),
                                     ImageFetcherCallback(), params);
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);
  ASSERT_TRUE(destroyed.Wait());
}

TEST_F(ImageFetcherImplTest, ImageCallbackCanDestroyFetcher) {
  ImageFetcherParams params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  base::test::TestFuture<void> destroyed;
  base::MockCallback<ImageFetcherCallback> cancelled;
  EXPECT_CALL(cancelled, Run(_, _)).Times(0);
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(),
      base::BindLambdaForTesting(
          [&](const gfx::Image&, const RequestMetadata&) {
            DestroyImageFetcher();
            destroyed.SetValue();
          }),
      params);
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(), cancelled.Get(), params);
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);
  ASSERT_TRUE(destroyed.Wait());
}

TEST_F(ImageFetcherImplTest, DifferentDataDecodersAreSeparate) {
  data_decoder::DataDecoder first_decoder;
  data_decoder::DataDecoder second_decoder;
  auto decoder = std::make_unique<MockImageDecoder>();
  auto finish_decode = [](const std::string&, const gfx::Size&,
                          data_decoder::DataDecoder*,
                          ImageDecodedCallback callback) {
    std::move(callback).Run(gfx::test::CreateImage(2, 3));
  };
  EXPECT_CALL(*decoder, DecodeImage(_, _, &first_decoder, _))
      .WillOnce(finish_decode);
  EXPECT_CALL(*decoder, DecodeImage(_, _, &second_decoder, _))
      .WillOnce(finish_decode);
  RecreateFetcherWithDecoder(std::move(decoder));

  ImageFetcherParams first_params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  first_params.set_data_decoder(&first_decoder);
  ImageFetcherParams second_params(TRAFFIC_ANNOTATION_FOR_TESTS, kImageUmaName);
  second_params.set_data_decoder(&second_decoder);
  base::test::TestFuture<gfx::Image, RequestMetadata> first;
  base::test::TestFuture<gfx::Image, RequestMetadata> second;
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(),
      first.GetCallback<const gfx::Image&, const RequestMetadata&>(),
      first_params);
  image_fetcher()->FetchImageAndData(
      GURL(kImageURL), ImageDataFetcherCallback(),
      second.GetCallback<const gfx::Image&, const RequestMetadata&>(),
      second_params);
  EXPECT_EQ(2, test_url_loader_factory()->NumPending());
  test_url_loader_factory()->AddResponse(kImageURL, kImageData);

  ASSERT_TRUE(first.Wait());
  ASSERT_TRUE(second.Wait());
  EXPECT_FALSE(first.Get<0>().IsEmpty());
  EXPECT_FALSE(second.Get<0>().IsEmpty());
  // The selected decoders must outlive the ImageDecoder owned by the fetcher.
  DestroyImageFetcher();
}

INSTANTIATE_TEST_SUITE_P(,
                         ImageFetcherRequestOptionsTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SmallFirst" : "LargeFirst";
                         });

}  // namespace
}  // namespace image_fetcher
