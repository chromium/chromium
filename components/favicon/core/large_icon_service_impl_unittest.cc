// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service_impl.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
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
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
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
using testing::StartsWith;

const char kDummyPageUrl[] = "http://www.example.com";
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

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult(int w,
                                                            int h,
                                                            SkColor color) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  // Create bitmap and fill with `color`.
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  gfx::PNGCodec::EncodeBGRASkBitmap(gfx::test::CreateBitmap(w, h, color), false,
                                    &data->as_vector());
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
      : mock_image_fetcher_(new NiceMock<MockImageFetcher>()),
        large_icon_service_(&mock_favicon_service_,
                            base::WrapUnique(mock_image_fetcher_.get()),
                            /*desired_size_in_dip_for_server_requests=*/24,
                            /*icon_type_for_server_requests=*/
                            favicon_base::IconType::kTouchIcon,
                            /*google_server_client_param=*/"test_chrome") {}

  LargeIconServiceTest(const LargeIconServiceTest&) = delete;
  LargeIconServiceTest& operator=(const LargeIconServiceTest&) = delete;

  ~LargeIconServiceTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  ui::test::ScopedSetSupportedResourceScaleFactors
      scoped_set_supported_scale_factors_{{ui::k200Percent}};
  raw_ptr<NiceMock<MockImageFetcher>, DanglingUntriaged> mock_image_fetcher_;
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
              CanSetOnDemandFavicons(GURL(kDummyPageUrl),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([](auto, auto, base::OnceCallback<void(bool)> callback) {
        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;
  EXPECT_CALL(*mock_image_fetcher_,
              FetchImageAndData_(kExpectedServerUrl, _, _, _))
      .WillOnce(
          PostFetchReply(gfx::test::CreateImage(/*size=*/64, kTestColor)));
  EXPECT_CALL(mock_favicon_service_,
              SetOnDemandFavicons(GURL(kDummyPageUrl), kExpectedServerUrl,
                                  favicon_base::IconType::kTouchIcon, _, _))
      .WillOnce(
          [](auto, auto, auto, auto, base::OnceCallback<void(bool)> callback) {
            return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), true));
          });

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyPageUrl),
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          callback.Get());

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
              CanSetOnDemandFavicons(GURL(kDummyPageUrl),
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
          gfx::test::CreateImage(/*size=*/64, kTestColor), expected_metadata));
  EXPECT_CALL(mock_favicon_service_,
              SetOnDemandFavicons(GURL(kDummyPageUrl), kExpectedOriginalUrl,
                                  favicon_base::IconType::kTouchIcon, _, _))
      .WillOnce(
          [](auto, auto, auto, auto, base::OnceCallback<void(bool)> callback) {
            return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), true));
          });

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;
  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyPageUrl),
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          callback.Get());

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
      .WillOnce(
          PostFetchReply(gfx::test::CreateImage(/*size=*/64, kTestColor)));
  // Verify that the non-trimmed page URL is used when writing to the database.
  EXPECT_CALL(mock_favicon_service_,
              SetOnDemandFavicons(_, kExpectedServerUrl, _, _, _));

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyUrlWithQuery),
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          favicon_base::GoogleFaviconServerCallback());

  task_environment_.RunUntilIdle();
}

TEST_F(LargeIconServiceTest, ShouldNotQueryGoogleServerIfInvalidScheme) {
  const GURL kDummyFtpUrl("ftp://www.example.com");

  EXPECT_CALL(*mock_image_fetcher_, FetchImageAndData_).Times(0);

  base::MockCallback<favicon_base::GoogleFaviconServerCallback> callback;

  large_icon_service_
      .GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
          GURL(kDummyFtpUrl),
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          callback.Get());

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
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          callback.Get());

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
              CanSetOnDemandFavicons(GURL(kDummyPageUrl),
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
          GURL(kDummyPageUrl),
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          callback.Get());

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
          GURL(kDummyPageUrl),
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          callback.Get());

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
              CanSetOnDemandFavicons(GURL(kDummyPageUrl),
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
          GURL(kDummyPageUrl),
          /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
          callback.Get());

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
  struct MockDatabaseLookupResult {
    GURL page_url;
    favicon_base::FaviconRawBitmapResult result;
  };

  struct MockDownloadResult {
    GURL image_url;
    size_t image_size = 0;
  };

  LargeIconServiceGetterTest() {
    ON_CALL(mock_favicon_service_,
            GetLargestRawFaviconForPageURL(_, _, _, _, _))
        .WillByDefault([this](const GURL& page_url, auto, auto,
                              favicon_base::FaviconRawBitmapCallback callback,
                              base::CancelableTaskTracker* tracker) {
          MockDatabaseLookupResult mock_result =
              (mock_database_lookup_result_.page_url == page_url)
                  ? mock_database_lookup_result_
                  : MockDatabaseLookupResult();
          return tracker->PostTask(
              base::SingleThreadTaskRunner::GetCurrentDefault().get(),
              FROM_HERE,
              base::BindOnce(std::move(callback), mock_result.result));
        });

    ON_CALL(*mock_image_fetcher_,
            FetchImageAndData_(
                Property("spec", &GURL::spec,
                         StartsWith("https://t0.gstatic.com/faviconV2")),
                _, _, _))
        .WillByDefault([this](auto, auto,
                              image_fetcher::ImageFetcherCallback* callback,
                              auto) {
          image_fetcher::RequestMetadata metadata;
          metadata.content_location_header =
              mock_download_result_.image_url.spec();
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(*callback),
                             gfx::test::CreateImage(
                                 mock_download_result_.image_size, kTestColor),
                             metadata));
        });

    ON_CALL(mock_favicon_service_,
            SetOnDemandFavicons(_, _, favicon_base::IconType::kTouchIcon, _, _))
        .WillByDefault([this](auto url, auto original_icon_url, auto icon_type,
                              const gfx::Image& image,
                              base::OnceCallback<void(bool)> callback) {
          // Simulate persisting the fetched icon in the local cache. Avoid
          // the complexity of transferring the data from the `image` because
          // the tests don't care about it.
          this->InjectMockDatabaseResult(
              url, CreateTestBitmapResult(image.Width(), image.Height(),
                                          kTestColor));
          return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), true));
        });
  }

  LargeIconServiceGetterTest(const LargeIconServiceGetterTest&) = delete;
  LargeIconServiceGetterTest& operator=(const LargeIconServiceGetterTest&) =
      delete;

  ~LargeIconServiceGetterTest() override = default;

  void ExpectFetchImageFromGoogleServer() {
    EXPECT_CALL(*mock_image_fetcher_,
                FetchImageAndData_(
                    Property("spec", &GURL::spec,
                             StartsWith("https://t0.gstatic.com/faviconV2")),
                    _, _, _));
  }

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
      returned_bitmap_size_ = result.bitmap.pixel_size;
    }
    StoreFallbackStyle(result.fallback_icon_style.get());
  }

  void ImageResultCallback(const favicon_base::LargeIconImageResult& result) {
    if (!result.image.IsEmpty()) {
      returned_bitmap_size_ = result.image.ToImageSkia()->size();
      ASSERT_TRUE(result.icon_url.is_valid());
    }
    StoreFallbackStyle(result.fallback_icon_style.get());
  }

  void StoreFallbackStyle(
      const favicon_base::FallbackIconStyle* fallback_style) {
    if (fallback_style) {
      returned_fallback_style_ = *fallback_style;
    }
  }

  void InjectMockDatabaseResult(
      const GURL& page_url,
      const favicon_base::FaviconRawBitmapResult& mock_result) {
    mock_database_lookup_result_.page_url = page_url;
    mock_database_lookup_result_.result = mock_result;
  }

  void InjectMockDownloadResult(GURL image_url, size_t image_size) {
    mock_download_result_.image_url = image_url;
    mock_download_result_.image_size = image_size;
  }

 protected:
  base::CancelableTaskTracker cancelable_task_tracker_;
  MockDatabaseLookupResult mock_database_lookup_result_;
  MockDownloadResult mock_download_result_;

  std::optional<favicon_base::FallbackIconStyle> returned_fallback_style_;
  std::optional<gfx::Size> returned_bitmap_size_;
};

// Test that `GetLargeIconFromCacheFallbackToGoogleServer()` returns the locally
// stored icon if the requested size matches the stored icon size. Verify that
// the Google server is not queried.
TEST_P(LargeIconServiceGetterTest,
       ShouldReturnIconFromLocalCacheIfSameSizeAvailable) {
  // TODO(crbug.com/337714411): Remove
  // LargeIconService::GetLargeIconImageOrFallbackStyleForPageUrl().
  if (GetParam()) {
    return;
  }
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(16, 16, kTestColor));

  EXPECT_CALL(mock_favicon_service_, CanSetOnDemandFavicons).Times(0);
  EXPECT_CALL(*mock_image_fetcher_, FetchImageAndData_).Times(0);

  large_icon_service_.GetLargeIconFromCacheFallbackToGoogleServer(
      GURL(kDummyPageUrl),
      /*min_source_size=*/LargeIconService::StandardIconSize::k16x16,
      std::nullopt, LargeIconService::NoBigEnoughIconBehavior::kReturnEmpty,
      /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindRepeating(&LargeIconServiceGetterTest::RawBitmapResultCallback,
                          base::Unretained(this)),
      &cancelable_task_tracker_);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(gfx::Size(16, 16), returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

// Test that `GetLargeIconFromCacheFallbackToGoogleServer()` resizes the locally
// stored icon if the requested size is smaller then the database icon size.
// Verify that the Google server is not queried.
TEST_P(LargeIconServiceGetterTest,
       ShouldReturnResizedIconFromLocalCacheIfLargerSizeAvailable) {
  // TODO(crbug.com/337714411): Remove
  // LargeIconService::GetLargeIconImageOrFallbackStyleForPageUrl().
  if (GetParam()) {
    return;
  }

  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(16, 16, kTestColor));

  EXPECT_CALL(mock_favicon_service_, CanSetOnDemandFavicons).Times(0);
  EXPECT_CALL(*mock_image_fetcher_, FetchImageAndData_).Times(0);

  large_icon_service_.GetLargeIconFromCacheFallbackToGoogleServer(
      GURL(kDummyPageUrl),
      /*min_source_size=*/LargeIconService::StandardIconSize::k16x16,
      /*size_to_resize_to=*/LargeIconService::StandardIconSize::k16x16,
      LargeIconService::NoBigEnoughIconBehavior::kReturnEmpty,
      /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindRepeating(&LargeIconServiceGetterTest::RawBitmapResultCallback,
                          base::Unretained(this)),
      &cancelable_task_tracker_);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(gfx::Size(16, 16), returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

// Test that `GetLargeIconFromCacheFallbackToGoogleServer()` doesn't query the
// Google server if the database icon has a smaller size than the passed-in
// `min_source_size`.
TEST_P(LargeIconServiceGetterTest,
       ShouldReturnResizedIconFromServerIfCachedIconIsTooSmall) {
  // TODO(crbug.com/337714411): Remove
  // LargeIconService::GetLargeIconImageOrFallbackStyleForPageUrl().
  if (GetParam()) {
    return;
  }
  // Inject an icon which size is smaller than the requested one.
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(16, 16, kTestColor));

  // Configure the Google server to return a 64x64 icon.
  const GURL kExpectedOriginalUrl("http://www.example.com/favicon.png");

  EXPECT_CALL(mock_favicon_service_, CanSetOnDemandFavicons).Times(0);
  EXPECT_CALL(*mock_image_fetcher_, FetchImageAndData_).Times(0);
  EXPECT_CALL(mock_favicon_service_, SetOnDemandFavicons).Times(0);

  // Request the icon of size exactly 32x32. The database 16x16 icon should not
  // be returned because `NoBigEnoughIconBehavior::kReturnEmpty` is used.
  large_icon_service_.GetLargeIconFromCacheFallbackToGoogleServer(
      GURL(kDummyPageUrl),
      /*min_source_size=*/LargeIconService::StandardIconSize::k32x32,
      /*size_to_resize_to=*/LargeIconService::StandardIconSize::k32x32,
      LargeIconService::NoBigEnoughIconBehavior::kReturnEmpty,
      /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindRepeating(&LargeIconServiceGetterTest::RawBitmapResultCallback,
                          base::Unretained(this)),
      &cancelable_task_tracker_);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(std::nullopt, returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

// Test that `GetLargeIconFromCacheFallbackToGoogleServer()` queries the Google
// server if there is no icon available locally for the specified `page_url`.
TEST_P(LargeIconServiceGetterTest,
       ShouldReturnResizedIconFromServerIfNoIconInCache) {
  // TODO(crbug.com/337714411): Remove
  // LargeIconService::GetLargeIconImageOrFallbackStyleForPageUrl().
  if (GetParam()) {
    return;
  }

  // Configure `FaviconService` to return no icon for the `kDummyPageUrl`.
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           favicon_base::FaviconRawBitmapResult());

  // Configure the Google server to return a 64x64 icon.
  const GURL kExpectedOriginalUrl("http://www.example.com/favicon.png");
  InjectMockDownloadResult(kExpectedOriginalUrl, /*image_size=*/64);

  EXPECT_CALL(mock_favicon_service_,
              CanSetOnDemandFavicons(GURL(kDummyPageUrl),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([](auto, auto, base::OnceCallback<void(bool)> callback) {
        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });

  ExpectFetchImageFromGoogleServer();

  EXPECT_CALL(mock_favicon_service_,
              SetOnDemandFavicons(GURL(kDummyPageUrl), kExpectedOriginalUrl,
                                  favicon_base::IconType::kTouchIcon, _, _));

  // Request the icon of size exactly 32x32 for the domain, which has no icons
  // stored locally.
  large_icon_service_.GetLargeIconFromCacheFallbackToGoogleServer(
      GURL(kDummyPageUrl),
      /*min_source_size=*/LargeIconService::StandardIconSize::k32x32,
      /*size_to_resize_to=*/LargeIconService::StandardIconSize::k32x32,
      LargeIconService::NoBigEnoughIconBehavior::kReturnBitmap,
      /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindRepeating(&LargeIconServiceGetterTest::RawBitmapResultCallback,
                          base::Unretained(this)),
      &cancelable_task_tracker_);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(gfx::Size(32, 32), returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

// Test that GetLargeIconFromCacheFallbackToGoogleServer() doesn't crash if the
// CancelableTaskTracker is destroyed before the async call finishes.
TEST_P(LargeIconServiceGetterTest, CancelableTaskTrackerDestroyedEarly) {
  // TODO(crbug.com/337714411): Remove
  // LargeIconService::GetLargeIconImageOrFallbackStyleForPageUrl().
  if (GetParam()) {
    return;
  }

  const GURL kIconUrl("http://www.example.com/favicon.png");

  auto test_specific_cancelable_task_tracker =
      std::make_unique<base::CancelableTaskTracker>();

  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           favicon_base::FaviconRawBitmapResult());
  InjectMockDownloadResult(kIconUrl, /*image_size=*/64);

  EXPECT_CALL(mock_favicon_service_,
              GetLargestRawFaviconForPageURL(_, _, _, _, _));

  // CancelableTaskTracker is destroyed during
  // FaviconService::CanSetOnDemandFavicons() call.
  EXPECT_CALL(mock_favicon_service_,
              CanSetOnDemandFavicons(GURL(kDummyPageUrl),
                                     favicon_base::IconType::kTouchIcon, _))
      .WillOnce([&test_specific_cancelable_task_tracker](
                    auto, auto, base::OnceCallback<void(bool)> callback) {
        test_specific_cancelable_task_tracker.reset();

        return base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), true));
      });

  ExpectFetchImageFromGoogleServer();

  // Request the icon of size exactly 32x32 for the domain, which has no icons
  // stored locally.
  large_icon_service_.GetLargeIconFromCacheFallbackToGoogleServer(
      GURL(kDummyPageUrl),
      /*min_source_size=*/LargeIconService::StandardIconSize::k32x32,
      /*size_to_resize_to=*/LargeIconService::StandardIconSize::k32x32,
      LargeIconService::NoBigEnoughIconBehavior::kReturnBitmap,
      /*should_trim_page_url_path=*/false, TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindRepeating(&LargeIconServiceGetterTest::RawBitmapResultCallback,
                          base::Unretained(this)),
      test_specific_cancelable_task_tracker.get());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(std::nullopt, returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

TEST_P(LargeIconServiceGetterTest, SameSize) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(24, 24, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(
      GURL(kDummyPageUrl),
      24,   // `min_source_size_in_pixel`
      24);  // `desired_size_in_pixel`
  EXPECT_EQ(gfx::Size(24, 24), returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

TEST_P(LargeIconServiceGetterTest, ScaleDown) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(32, 32, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyPageUrl), 24, 24);
  EXPECT_EQ(gfx::Size(24, 24), returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

TEST_P(LargeIconServiceGetterTest, ScaleUp) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(16, 16, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(
      GURL(kDummyPageUrl),
      14,  // Lowered requirement so stored bitmap is admitted.
      24);
  EXPECT_EQ(gfx::Size(24, 24), returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

// `desired_size_in_pixel` == 0 means retrieve original image without scaling.
TEST_P(LargeIconServiceGetterTest, NoScale) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(24, 24, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyPageUrl), 16, 0);
  EXPECT_EQ(gfx::Size(24, 24), returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

TEST_P(LargeIconServiceGetterTest, FallbackSinceIconTooSmall) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(16, 16, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyPageUrl), 24, 24);
  EXPECT_EQ(std::nullopt, returned_bitmap_size_);
  EXPECT_TRUE(HasBackgroundColor(*returned_fallback_style_, kTestColor));
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       16, /*expected_count=*/1);
}

TEST_P(LargeIconServiceGetterTest, FallbackSinceIconNotSquare) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(24, 32, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyPageUrl), 24, 24);
  EXPECT_EQ(std::nullopt, returned_bitmap_size_);
  EXPECT_TRUE(HasBackgroundColor(*returned_fallback_style_, kTestColor));
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       24, /*expected_count=*/1);
}

TEST_P(LargeIconServiceGetterTest, FallbackSinceIconMissing) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           favicon_base::FaviconRawBitmapResult());
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyPageUrl), 24, 24);
  EXPECT_EQ(std::nullopt, returned_bitmap_size_);
  EXPECT_TRUE(returned_fallback_style_->is_default_background_color);
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       0, /*expected_count=*/1);
}

TEST_P(LargeIconServiceGetterTest, FallbackSinceIconMissingNoScale) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           favicon_base::FaviconRawBitmapResult());
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyPageUrl), 24, 0);
  EXPECT_EQ(std::nullopt, returned_bitmap_size_);
  EXPECT_TRUE(returned_fallback_style_->is_default_background_color);
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       0, /*expected_count=*/1);
}

// Oddball case where we demand a high resolution icon to scale down. Generates
// fallback even though an icon with the final size is available.
TEST_P(LargeIconServiceGetterTest, FallbackSinceTooPicky) {
  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(24, 24, kTestColor));
  GetLargeIconOrFallbackStyleAndWaitForCallback(GURL(kDummyPageUrl), 32, 24);
  EXPECT_EQ(std::nullopt, returned_bitmap_size_);
  EXPECT_TRUE(HasBackgroundColor(*returned_fallback_style_, kTestColor));
  histogram_tester_.ExpectUniqueSample("Favicons.LargeIconService.FallbackSize",
                                       24, /*expected_count=*/1);
}

TEST_P(LargeIconServiceGetterTest, IconTooSmallStillWantBitmap) {
  // TODO(crbug.com/337714411): Remove
  // LargeIconService::GetLargeIconImageOrFallbackStyleForPageUrl().
  if (GetParam()) {
    return;
  }

  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(16, 16, kTestColor));
  large_icon_service_.GetLargeIconRawBitmapForPageUrl(
      GURL(kDummyPageUrl), 24, std::nullopt,
      LargeIconService::NoBigEnoughIconBehavior::kReturnBitmap,
      base::BindRepeating(&LargeIconServiceGetterTest::RawBitmapResultCallback,
                          base::Unretained(this)),
      &cancelable_task_tracker_);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(gfx::Size(16, 16), returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

TEST_P(LargeIconServiceGetterTest, IconTooSmallDontWantAnything) {
  // TODO(crbug.com/337714411): Remove
  // LargeIconService::GetLargeIconImageOrFallbackStyleForPageUrl().
  if (GetParam()) {
    return;
  }

  InjectMockDatabaseResult(GURL(kDummyPageUrl),
                           CreateTestBitmapResult(16, 16, kTestColor));
  large_icon_service_.GetLargeIconRawBitmapForPageUrl(
      GURL(kDummyPageUrl), 24, std::nullopt,
      LargeIconService::NoBigEnoughIconBehavior::kReturnEmpty,
      base::BindRepeating(&LargeIconServiceGetterTest::RawBitmapResultCallback,
                          base::Unretained(this)),
      &cancelable_task_tracker_);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(std::nullopt, returned_bitmap_size_);
  EXPECT_EQ(std::nullopt, returned_fallback_style_);
}

// Every test will appear with suffix /0 (param false) and /1 (param true), e.g.
//  LargeIconServiceGetterTest.FallbackSinceTooPicky/0: get image.
//  LargeIconServiceGetterTest.FallbackSinceTooPicky/1: get raw bitmap.
INSTANTIATE_TEST_SUITE_P(All,  // Empty instatiation name.
                         LargeIconServiceGetterTest,
                         ::testing::Values(false, true));

}  // namespace
}  // namespace favicon
