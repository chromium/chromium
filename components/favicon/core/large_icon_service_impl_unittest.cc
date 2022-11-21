// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/layout.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace favicon {
namespace {

using image_fetcher::MockImageFetcher;
using testing::_;
using testing::Eq;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::IsNull;
using testing::NiceMock;
using testing::Not;
using testing::Property;
using testing::Return;
using testing::SaveArg;

const char kDummyUrl[] = "http://www.example.com";
const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";
const SkColor kTestColor = SK_ColorRED;

ACTION_P(PostFetchReply, p0) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*arg2), p0, image_fetcher::RequestMetadata()));
}

ACTION_P2(PostFetchReplyWithMetadata, p0, p1) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*arg2), p0, p1));
}

SkBitmap CreateTestSkBitmap(int w, int h, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(w, h);
  bitmap.eraseColor(color);
  return bitmap;
}

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult(int w,
                                                            int h,
                                                            SkColor color) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  // Create bitmap and fill with |color|.
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  SkBitmap bitmap;
  bitmap.allocN32Pixels(w, h);
  bitmap.eraseColor(color);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->data());
  result.bitmap_data = data;

  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyIconUrl);
  result.icon_type = favicon_base::IconType::kTouchIcon;
  CHECK(result.is_valid());
  return result;
}

bool HasBackgroundColor(
    const favicon_base::FallbackIconStyle& fallback_icon_style,
    SkColor color) {
  return !fallback_icon_style.is_default_background_color &&
         fallback_icon_style.background_color == color;
}

// TODO(jkrcal): Make the tests a bit crisper, see crbug.com/725822.
class LargeIconServiceTest : public testing::Test {
 public:
  LargeIconServiceTest()
      : scoped_set_supported_scale_factors_({ui::k200Percent}),
        mock_image_fetcher_(new NiceMock<MockImageFetcher>()),
        large_icon_service_(&mock_favicon_service_,
                            base::WrapUnique(mock_image_fetcher_.get()),
                            /*desired_size_in_dip_for_server_requests=*/24,
                            /*icon_type_for_server_requests=*/
                            favicon_base::IconType::kTouchIcon,
                            /*google_server_client_param=*/"test_chrome") {}

  LargeIconServiceTest(const LargeIconServiceTest&) = delete;
  LargeIconServiceTest& operator=(const LargeIconServiceTest&) = delete;

  ~LargeIconServiceTest() override {}

 protected:
  base::test::TaskEnvironment task_environment_;
  ui::test::ScopedSetSupportedResourceScaleFactors
      scoped_set_supported_scale_factors_;
  raw_ptr<NiceMock<MockImageFetcher>> mock_image_fetcher_;
  testing::NiceMock<MockFaviconService> mock_favicon_service_;
  LargeIconServiceImpl large_icon_service_;
  base::HistogramTester histogram_tester_;
};

TEST_F(LargeIconServiceTest, ShouldGetFromGoogleServer) {
  const GURL kExpectedServerUrl(
      "https://t0.gstatic.com/faviconV2?client=test_chrome&nfrp=2"
      "&check_seen=true&size=48&min_size=16&max_size=256"
      "&fallback_opts=TYPE,SIZE,URL&url=http://www.example.com/");

  EXPECT_CALL(mock_favicon_service_, UnableToDownloadFavicon).Times(0);
  EXPECT_CALL(mock_favicon_service_,
              CanSetOnDemandFavicons(GURL(kDummyUrl),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([](auto, auto, base::OnceCallback<void(bool)> callback) {
        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;
  EXPECT_CALL(*mock_image_fetcher_,
              FetchImageAndData_(kExpectedServerUrl, _, _, _))
      .WillOnce(PostFetchReply(gfx::Image::CreateFrom1xBitmap(
          CreateTestSkBitmap(64, 64, kTestColor))));
  EXPECT_CALL(mock_favicon_service_,
              SetOnDemandFavicons(GURL(kDummyUrl), kExpectedServerUrl,
                                  favicon_base::IconType::kTouchIcon, _, _))
      .WillOnce(
          [](auto, auto, auto, auto, base::OnceCallback<void(bool)> callback) {
            return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), true));
          });

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyUrl),
          /*may_page_url_be_private=*/true, /*should_trim_page_url_path=*/false,
          TRAFFIC_ANNOTATION_FOR_TESTS, callback.Get());

  EXPECT_CALL(callback,
              Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS));
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "Favicons.LargeIconService.DownloadedSize", 64, /*expected_count=*/1);
}

TEST_F(LargeIconServiceTest, ShouldGetFromGoogleServerWithOriginalUrl) {
  const GURL kExpectedServerUrl(
      "https://t0.gstatic.com/faviconV2?client=test_chrome&nfrp=2"
      "&check_seen=true&size=48&min_size=16&max_size=256"
      "&fallback_opts=TYPE,SIZE,URL&url=http://www.example.com/");
  const GURL kExpectedOriginalUrl("http://www.example.com/favicon.png");

  EXPECT_CALL(mock_favicon_service_,
              CanSetOnDemandFavicons(GURL(kDummyUrl),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([](auto, auto, base::OnceCallback<void(bool)> callback) {
        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });

  image_fetcher::RequestMetadata expected_metadata;
  expected_metadata.content_location_header = kExpectedOriginalUrl.spec();
  EXPECT_CALL(*mock_image_fetcher_,
              FetchImageAndData_(kExpectedServerUrl, _, _, _))
      .WillOnce(PostFetchReplyWithMetadata(
          gfx::Image::CreateFrom1xBitmap(
              CreateTestSkBitmap(64, 64, kTestColor)),
          expected_metadata));
  EXPECT_CALL(mock_favicon_service_,
              SetOnDemandFavicons(GURL(kDummyUrl), kExpectedOriginalUrl,
                                  favicon_base::IconType::kTouchIcon, _, _))
      .WillOnce(
          [](auto, auto, auto, auto, base::OnceCallback<void(bool)> callback) {
            return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), true));
          });

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;
  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyUrl),
          /*may_page_url_be_private=*/true, /*should_trim_page_url_path=*/false,
          TRAFFIC_ANNOTATION_FOR_TESTS, callback.Get());

  EXPECT_CALL(callback,
              Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS));
  task_environment_.RunUntilIdle();
}

TEST_F(LargeIconServiceTest, ShouldTrimQueryParametersForGoogleServer) {
  const GURL kDummyUrlWithQuery("http://www.example.com?foo=1");
  const GURL kExpectedServerUrl(
      "https://t0.gstatic.com/faviconV2?client=test_chrome&nfrp=2"
      "&check_seen=true&size=48&min_size=16&max_size=256"
      "&fallback_opts=TYPE,SIZE,URL&url=http://www.example.com/");

  EXPECT_CALL(mock_favicon_service_,
              CanSetOnDemandFavicons(GURL(kDummyUrlWithQuery),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([](auto, auto, base::OnceCallback<void(bool)> callback) {
        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });

  EXPECT_CALL(*mock_image_fetcher_,
              FetchImageAndData_(kExpectedServerUrl, _, _, _))
      .WillOnce(PostFetchReply(gfx::Image::CreateFrom1xBitmap(
          CreateTestSkBitmap(64, 64, kTestColor))));
  // Verify that the non-trimmed page URL is used when writing to the database.
  EXPECT_CALL(mock_favicon_service_,
              SetOnDemandFavicons(_, kExpectedServerUrl, _, _, _));

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyUrlWithQuery),
          /*may_page_url_be_private=*/true, /*should_trim_page_url_path=*/false,
          TRAFFIC_ANNOTATION_FOR_TESTS,
          favicon_base::GoogleFaviconServerCallback());

  task_environment_.RunUntilIdle();
}

TEST_F(LargeIconServiceTest, ShouldNotCheckOnPublicUrls) {
  EXPECT_CALL(mock_favicon_service_,
              CanSetOnDemandFavicons(GURL(kDummyUrl),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([](auto, auto, base::OnceCallback<void(bool)> callback) {
        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });

  // The request has no "check_seen=true"; full URL is tested elsewhere.
  EXPECT_CALL(
      *mock_image_fetcher_,
      FetchImageAndData_(
          Property(&GURL::query, Not(HasSubstr("check_seen=true"))), _, _, _))
      .WillOnce(PostFetchReply(gfx::Image()));

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyUrl),
          /*may_page_url_be_private=*/false,
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          callback.Get());

  EXPECT_CALL(callback, Run(favicon_base::GoogleFaviconServerRequestStatus::
                                FAILURE_CONNECTION_ERROR));
  task_environment_.RunUntilIdle();
}

TEST_F(LargeIconServiceTest, ShouldNotQueryGoogleServerIfInvalidScheme) {
  const GURL kDummyFtpUrl("ftp://www.example.com");

  EXPECT_CALL(*mock_image_fetcher_, FetchImageAndData_).Times(0);

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyFtpUrl),
          /*may_page_url_be_private=*/true, /*should_trim_page_url_path=*/false,
          TRAFFIC_ANNOTATION_FOR_TESTS, callback.Get());

  EXPECT_CALL(callback, Run(favicon_base::GoogleFaviconServerRequestStatus::
                                FAILURE_TARGET_URL_SKIPPED));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Favicons.LargeIconService.DownloadedSize"),
              IsEmpty());
}

TEST_F(LargeIconServiceTest, ShouldNotQueryGoogleServerIfInvalidURL) {
  const GURL kDummyInvalidUrl("htt");

  EXPECT_CALL(*mock_image_fetcher_, FetchImageAndData_).Times(0);

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyInvalidUrl),
          /*may_page_url_be_private=*/true, /*should_trim_page_url_path=*/false,
          TRAFFIC_ANNOTATION_FOR_TESTS, callback.Get());

  EXPECT_CALL(callback, Run(favicon_base::GoogleFaviconServerRequestStatus::
                                FAILURE_TARGET_URL_INVALID));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Favicons.LargeIconService.DownloadedSize"),
              IsEmpty());
}

TEST_F(LargeIconServiceTest, ShouldReportUnavailableIfFetchFromServerFails) {
  const GURL kExpectedServerUrl(
      "https://t0.gstatic.com/faviconV2?client=test_chrome&nfrp=2"
      "&check_seen=true&size=48&min_size=16&max_size=256"
      "&fallback_opts=TYPE,SIZE,URL&url=http://www.example.com/");

  EXPECT_CALL(mock_favicon_service_,
              CanSetOnDemandFavicons(GURL(kDummyUrl),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([](auto, auto, base::OnceCallback<void(bool)> callback) {
        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });
  EXPECT_CALL(mock_favicon_service_, SetOnDemandFavicons).Times(0);

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;
  EXPECT_CALL(*mock_image_fetcher_,
              FetchImageAndData_(kExpectedServerUrl, _, _, _))
      .WillOnce(PostFetchReply(gfx::Image()));
  EXPECT_CALL(mock_favicon_service_,
              UnableToDownloadFavicon(kExpectedServerUrl));

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyUrl),
          /*may_page_url_be_private=*/true, /*should_trim_page_url_path=*/false,
          TRAFFIC_ANNOTATION_FOR_TESTS, callback.Get());

  EXPECT_CALL(callback, Run(favicon_base::GoogleFaviconServerRequestStatus::
                                FAILURE_CONNECTION_ERROR));
  task_environment_.RunUntilIdle();
  // Verify that download failure gets recorded.
  histogram_tester_.ExpectUniqueSample(
      "Favicons.LargeIconService.DownloadedSize", 0, /*expected_count=*/1);
}

TEST_F(LargeIconServiceTest, ShouldNotGetFromGoogleServerIfUnavailable) {
  ON_CALL(mock_favicon_service_,
          WasUnableToDownloadFavicon(
              GURL("https://t0.gstatic.com/faviconV2?client=test_chrome&nfrp=2"
                   "&check_seen=true&size=48&min_size=16&max_size=256"
                   "&fallback_opts=TYPE,SIZE,URL&url=http://www.example.com/")))
      .WillByDefault(Return(true));

  EXPECT_CALL(mock_favicon_service_, UnableToDownloadFavicon).Times(0);
  EXPECT_CALL(*mock_image_fetcher_, FetchImageAndData_).Times(0);
  EXPECT_CALL(mock_favicon_service_, SetOnDemandFavicons).Times(0);

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;
  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyUrl),
          /*may_page_url_be_private=*/true, /*should_trim_page_url_path=*/false,
          TRAFFIC_ANNOTATION_FOR_TESTS, callback.Get());

  EXPECT_CALL(callback, Run(favicon_base::GoogleFaviconServerRequestStatus::
                                FAILURE_HTTP_ERROR_CACHED));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Favicons.LargeIconService.DownloadedSize"),
              IsEmpty());
}

TEST_F(LargeIconServiceTest, ShouldNotGetFromGoogleServerIfCannotSet) {
  EXPECT_CALL(mock_favicon_service_, UnableToDownloadFavicon).Times(0);
  EXPECT_CALL(mock_favicon_service_,
              CanSetOnDemandFavicons(GURL(kDummyUrl),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([](auto, auto, base::OnceCallback<void(bool)> callback) {
        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), false));
      });

  EXPECT_CALL(*mock_image_fetcher_, FetchImageAndData_).Times(0);
  EXPECT_CALL(mock_favicon_service_, SetOnDemandFavicons).Times(0);

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;
  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyUrl),
          /*may_page_url_be_private=*/true, /*should_trim_page_url_path=*/false,
          TRAFFIC_ANNOTATION_FOR_TESTS, callback.Get());

  EXPECT_CALL(callback, Run(favicon_base::GoogleFaviconServerRequestStatus::
                                FAILURE_ICON_EXISTS_IN_DB));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(histogram_tester_.GetAllSamples(
                  "Favicons.LargeIconService.DownloadedSize"),
              IsEmpty());
}

class LargeIconServiceGetterTest : public LargeIconServiceTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  LargeIconServiceGetterTest() {}

  LargeIconServiceGetterTest(const LargeIconServiceGetterTest&) = delete;
  LargeIconServiceGetterTest& operator=(const LargeIconServiceGetterTest&) =
      delete;

  ~LargeIconServiceGetterTest() override {}

  void GetLargeIconOrFallbackStyleAndWaitForCallback(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel) {
    // Switch over testing two analogous functions based on the bool param.
    if (GetParam()) {
      large_icon_service_.GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
          page_url, min_source_size_in_pixel, desired_size_in_pixel,
          base::BindRepeating(
              &LargeIconServiceGetterTest::RawBitmapResultCallback,
              base::Unretained(this)),
          &cancelable_task_tracker_);
    } else {
      large_icon_service_.GetLargeIconImageOrFallbackStyleForPageUrl(
          page_url, min_source_size_in_pixel, desired_size_in_pixel,
          base::BindRepeating(&LargeIconServiceGetterTest::ImageResultCallback,
                              base::Unretained(this)),
          &cancelable_task_tracker_);
    }
    task_environment_.RunUntilIdle();
  }

  void RawBitmapResultCallback(const favicon_base::LargeIconResult& result) {
    if (result.bitmap.is_valid()) {
      returned_bitmap_size_ =
          std::make_unique<gfx::Size>(result.bitmap.pixel_size);
    }
    StoreFallbackStyle(result.fallback_icon_style.get());
  }

  void ImageResultCallback(const favicon_base::LargeIconImageResult& result) {
    if (!result.image.IsEmpty()) {
      returned_bitmap_size_ =
          std::make_unique<gfx::Size>(result.image.ToImageSkia()->size());
      ASSERT_TRUE(result.icon_url.is_valid());
    }
    StoreFallbackStyle(result.fallback_icon_style.get());
  }

  void StoreFallbackStyle(
      const favicon_base::FallbackIconStyle* fallback_style) {
    if (fallback_style) {
      returned_fallback_style_ =
          std::make_unique<favicon_base::FallbackIconStyle>(*fallback_style);
    }
  }

  // The parameter |mock_result| needs to be passed by value otherwise the
  // lambda injected into the mock may capture a reference to a temporary,
  // which would cause Undefined Behaviour.
  void InjectMockResult(const GURL& page_url,
                        favicon_base::FaviconRawBitmapResult mock_result) {
    ON_CALL(mock_favicon_service_,
            GetLargestRawFaviconForPageURL(page_url, _, _, _, _))
        .WillByDefault([=](auto, auto, auto,
                           favicon_base::FaviconRawBitmapCallback callback,
                           base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::SingleThreadTaskRunner::GetCurrentDefault().get(),
              FROM_HERE, base::BindOnce(std::move(callback), mock_result));
        });
  }

 protected:
  base::CancelableTaskTracker cancelable_task_tracker_;

  std::unique_ptr<favicon_base::FallbackIconStyle> returned_fallback_style_;
  std::unique_ptr<gfx::Size> returned_bitmap_size_;
};

TEST_P(LargeIconServiceGetterTest, SameSize) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmapResult(24, 24, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(
      GURL(kDummyUrl),
      24,   // |min_source_size_in_pixel|
      24);  // |desired_size_in_pixel|
  EXPECT_EQ(gfx::Size(24, 24), *returned_bitmap_size_);
  EXPECT_EQ(nullptr, returned_fallback_style_);
}

TEST_P(LargeIconServiceGetterTest, ScaleDown) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmapResult(32, 32, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyUrl), 24, 24);
  EXPECT_EQ(gfx::Size(24, 24), *returned_bitmap_size_);
  EXPECT_EQ(nullptr, returned_fallback_style_);
}

TEST_P(LargeIconServiceGetterTest, ScaleUp) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmapResult(16, 16, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(
      GURL(kDummyUrl),
      14,  // Lowered requirement so stored bitmap is admitted.
      24);
  EXPECT_EQ(gfx::Size(24, 24), *returned_bitmap_size_);
  EXPECT_EQ(nullptr, returned_fallback_style_);
}

// |desired_size_in_pixel| == 0 means retrieve original image without scaling.
TEST_P(LargeIconServiceGetterTest, NoScale) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmapResult(24, 24, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyUrl), 16, 0);
  EXPECT_EQ(gfx::Size(24, 24), *returned_bitmap_size_);
  EXPECT_EQ(nullptr, returned_fallback_style_);
}

TEST_P(LargeIconServiceGetterTest, FallbackSinceIconTooSmall) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmapResult(16, 16, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyUrl), 24, 24);
  EXPECT_EQ(nullptr, returned_bitmap_size_);
  EXPECT_TRUE(HasBackgroundColor(*returned_fallback_style_, kTestColor));
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       16, /*expected_count=*/1);
}

TEST_P(LargeIconServiceGetterTest, FallbackSinceIconNotSquare) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmapResult(24, 32, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyUrl), 24, 24);
  EXPECT_EQ(nullptr, returned_bitmap_size_);
  EXPECT_TRUE(HasBackgroundColor(*returned_fallback_style_, kTestColor));
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       24, /*expected_count=*/1);
}

TEST_P(LargeIconServiceGetterTest, FallbackSinceIconMissing) {
  InjectMockResult(GURL(kDummyUrl), favicon_base::FaviconRawBitmapResult());
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyUrl), 24, 24);
  EXPECT_EQ(nullptr, returned_bitmap_size_);
  EXPECT_TRUE(returned_fallback_style_->is_default_background_color);
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       0, /*expected_count=*/1);
}

TEST_P(LargeIconServiceGetterTest, FallbackSinceIconMissingNoScale) {
  InjectMockResult(GURL(kDummyUrl), favicon_base::FaviconRawBitmapResult());
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyUrl), 24, 0);
  EXPECT_EQ(nullptr, returned_bitmap_size_);
  EXPECT_TRUE(returned_fallback_style_->is_default_background_color);
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       0, /*expected_count=*/1);
}

// Oddball case where we demand a high resolution icon to scale down. Generates
// fallback even though an icon with the final size is available.
TEST_P(LargeIconServiceGetterTest, FallbackSinceTooPicky) {
  InjectMockResult(GURL(kDummyUrl), CreateTestBitmapResult(24, 24, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyUrl), 32, 24);
  EXPECT_EQ(nullptr, returned_bitmap_size_);
  EXPECT_TRUE(HasBackgroundColor(*returned_fallback_style_, kTestColor));
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       24, /*expected_count=*/1);
}

// Every test will appear with suffix /0 (param false) and /1 (param true), e.g.
//  LargeIconServiceGetterTest.FallbackSinceTooPicky/0: get image.
//  LargeIconServiceGetterTest.FallbackSinceTooPicky/1: get raw bitmap.
INSTANTIATE_TEST_SUITE_P(All,  // Empty instatiation name.
                         LargeIconServiceGetterTest,
                         ::testing::Values(false, true));

}  // namespace
}  // namespace favicon
