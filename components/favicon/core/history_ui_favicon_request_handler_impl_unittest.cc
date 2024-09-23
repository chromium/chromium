// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace favicon {
namespace {

using testing::_;
using testing::Return;

const base::CancelableTaskTracker::TaskId kTaskId = 1;

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult(
    const GURL& icon_url,
    int desired_size_in_pixel) {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  gfx::PNGCodec::EncodeBGRASkBitmap(
      gfx::test::CreateBitmap(desired_size_in_pixel, SK_ColorRED), false,
      &data->as_vector());
  favicon_base::FaviconRawBitmapResult result;
  result.bitmap_data = data;
  result.icon_url = icon_url;
  result.pixel_size = gfx::Size(desired_size_in_pixel, desired_size_in_pixel);
  return result;
}

favicon_base::FaviconImageResult CreateTestImageResult(const GURL& icon_url) {
  favicon_base::FaviconImageResult result;
  result.image = gfx::test::CreateImage(gfx::kFaviconSize, SK_ColorRED);
  result.icon_url = icon_url;
  return result;
}

void StoreBitmap(favicon_base::FaviconRawBitmapResult* destination,
                 const favicon_base::FaviconRawBitmapResult& result) {
  *destination = result;
}

void StoreImage(favicon_base::FaviconImageResult* destination,
                const favicon_base::FaviconImageResult& result) {
  *destination = result;
}

class MockFaviconServiceWithFake : public MockFaviconService {
 public:
  MockFaviconServiceWithFake() {
    // Fake won't respond with any icons at first.
    ON_CALL(*this, GetRawFaviconForPageURL)
        .WillByDefault([](auto, auto, auto, auto,
                          favicon_base::FaviconRawBitmapCallback callback,
                          auto) {
          std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
          return kTaskId;
        });
    ON_CALL(*this, GetFaviconImageForPageURL)
        .WillByDefault(
            [](auto, favicon_base::FaviconImageCallback callback, auto) {
              std::move(callback).Run(favicon_base::FaviconImageResult());
              return kTaskId;
            });
  }

  MockFaviconServiceWithFake(const MockFaviconServiceWithFake&) = delete;
  MockFaviconServiceWithFake& operator=(const MockFaviconServiceWithFake&) =
      delete;

  ~MockFaviconServiceWithFake() override = default;

  // Simulates the service having an icon stored for `page_url`, the URL of the
  // image being `icon_url`. The real FaviconService performs resizing if it
  // can't find a stored icon matching the requested size, so the same is true
  // here: any requested size will return a bitmap of that size.
  void StoreMockLocalFavicon(const GURL& page_url, const GURL& icon_url) {
    ON_CALL(*this, GetRawFaviconForPageURL(page_url, _, _, _, _, _))
        .WillByDefault(
            [icon_url](auto, auto, int desired_size_in_pixel, auto,
                       favicon_base::FaviconRawBitmapCallback callback, auto) {
              std::move(callback).Run(
                  CreateTestBitmapResult(icon_url, desired_size_in_pixel));
              return kTaskId;
            });
    ON_CALL(*this, GetFaviconImageForPageURL(page_url, _, _))
        .WillByDefault([icon_url](auto,
                                  favicon_base::FaviconImageCallback callback,
                                  auto) {
          std::move(callback).Run(CreateTestImageResult(icon_url));
          return kTaskId;
        });
  }
};

class MockLargeIconServiceWithFake : public LargeIconService {
 public:
  explicit MockLargeIconServiceWithFake(
      MockFaviconServiceWithFake* mock_favicon_service_with_fake)
      : mock_favicon_service_with_fake_(mock_favicon_service_with_fake) {
    // Fake won't respond with any icons at first (HTTP error 404).
    ON_CALL(*this,
            GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(_, _,
                                                                          _, _))
        .WillByDefault(
            [](auto, auto, auto,
               favicon_base::GoogleFaviconServerCallback server_callback) {
              std::move(server_callback)
                  .Run(favicon_base::GoogleFaviconServerRequestStatus::
                           FAILURE_HTTP_ERROR);
            });
  }

  MockLargeIconServiceWithFake(const MockLargeIconServiceWithFake&) = delete;
  MockLargeIconServiceWithFake& operator=(const MockLargeIconServiceWithFake&) =
      delete;

  ~MockLargeIconServiceWithFake() override = default;

  // Simulates the Google Server having an icon stored for `page_url`, of
  // associated `icon_url`. Requests will cause the icon to be stored in
  // `mock_favicon_service_with_fake_`.
  void StoreMockGoogleServerFavicon(const GURL& page_url,
                                    const GURL& icon_url) {
    ON_CALL(*this,
            GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(_, _,
                                                                          _, _))
        .WillByDefault(
            [=, this](
                auto, auto, auto,
                favicon_base::GoogleFaviconServerCallback server_callback) {
              mock_favicon_service_with_fake_->StoreMockLocalFavicon(page_url,
                                                                     icon_url);
              std::move(server_callback)
                  .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
            });
  }

  // LargeIconService overrides.
  MOCK_METHOD(void,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
              (const GURL& page_url,
               bool should_trim_page_url_path,
               const net::NetworkTrafficAnnotationTag& traffic_annotation,
               favicon_base::GoogleFaviconServerCallback callback),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconImageOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconImageCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(
      base::CancelableTaskTracker::TaskId,
      GetLargeIconRawBitmapForPageUrl,
      (const GURL& page_url,
       int min_source_size_in_pixel,
       std::optional<int> size_in_pixel_to_resize_to,
       LargeIconService::NoBigEnoughIconBehavior no_big_enough_icon_behavior,
       favicon_base::LargeIconCallback callback,
       base::CancelableTaskTracker* tracker),
      (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
              (const GURL& icon_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetIconRawBitmapOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(void,
              GetLargeIconFromCacheFallbackToGoogleServer,
              (const GURL& page_url,
               StandardIconSize min_source_size_in_pixel,
               std::optional<StandardIconSize> size_in_pixel_to_resize_to,
               NoBigEnoughIconBehavior no_big_enough_icon_behavior,
               bool should_trim_page_url_path,
               const net::NetworkTrafficAnnotationTag& traffic_annotation,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(void,
              TouchIconFromGoogleServer,
              (const GURL& icon_url),
              (override));

 private:
  const raw_ptr<MockFaviconServiceWithFake> mock_favicon_service_with_fake_;
};

class HistoryUiFaviconRequestHandlerImplTest : public ::testing::Test {
 public:
  HistoryUiFaviconRequestHandlerImplTest()
      : mock_large_icon_service_(&mock_favicon_service_),
        history_ui_favicon_request_handler_(can_send_history_data_getter_.Get(),
                                            &mock_favicon_service_,
                                            &mock_large_icon_service_) {
    // Allow sending history data by default.
    ON_CALL(can_send_history_data_getter_, Run()).WillByDefault(Return(true));
  }

  HistoryUiFaviconRequestHandlerImplTest(
      const HistoryUiFaviconRequestHandlerImplTest&) = delete;
  HistoryUiFaviconRequestHandlerImplTest& operator=(
      const HistoryUiFaviconRequestHandlerImplTest&) = delete;

 protected:
  testing::NiceMock<MockFaviconServiceWithFake> mock_favicon_service_;
  testing::NiceMock<MockLargeIconServiceWithFake> mock_large_icon_service_;
  testing::NiceMock<base::MockCallback<
      HistoryUiFaviconRequestHandlerImpl::CanSendHistoryDataGetter>>
      can_send_history_data_getter_;
  base::HistogramTester histogram_tester_;
  HistoryUiFaviconRequestHandlerImpl history_ui_favicon_request_handler_;

  // Convenience constants used in the tests.
  const GURL kPageUrl = GURL("https://www.example.com");
  const GURL kIconUrl = GURL("https://www.example.com/favicon16.png");
  const HistoryUiFaviconRequestOrigin kOrigin =
      HistoryUiFaviconRequestOrigin::kHistory;
  const std::string kOriginHistogramSuffix = ".HISTORY";
  const std::string kAvailabilityHistogramName =
      "Sync.SyncedHistoryFaviconAvailability";
};

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetEmptyBitmap) {
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(kPageUrl, _,
                                      /*desired_size_in_pixel=*/16, _, _, _));
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      kPageUrl, /*desired_size_in_pixel=*/16,
      base::BindOnce(&StoreBitmap, &result), kOrigin);
  EXPECT_FALSE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      kAvailabilityHistogramName + kOriginHistogramSuffix,
      FaviconAvailability::kNotAvailable, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetLocalBitmap) {
  mock_favicon_service_.StoreMockLocalFavicon(kPageUrl, kIconUrl);
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(kPageUrl, _,
                                      /*desired_size_in_pixel=*/16, _, _, _));
  EXPECT_CALL(mock_large_icon_service_, TouchIconFromGoogleServer(kIconUrl));
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      kPageUrl, /*desired_size_in_pixel=*/16,
      base::BindOnce(&StoreBitmap, &result), kOrigin);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      kAvailabilityHistogramName + kOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetGoogleServerBitmap) {
  mock_large_icon_service_.StoreMockGoogleServerFavicon(kPageUrl, kIconUrl);
  EXPECT_CALL(can_send_history_data_getter_, Run());
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(kPageUrl, _,
                                      /*desired_size_in_pixel=*/16, _, _, _))
      .Times(2);
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  kPageUrl,
                  /*should_trim_page_url_path=*/false, _, _));
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      kPageUrl, /*desired_size_in_pixel=*/16,
      base::BindOnce(&StoreBitmap, &result), kOrigin);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      kAvailabilityHistogramName + kOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetEmptyImage) {
  EXPECT_CALL(mock_favicon_service_, GetFaviconImageForPageURL(kPageUrl, _, _));
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      kPageUrl, base::BindOnce(&StoreImage, &result), kOrigin);
  EXPECT_TRUE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      kAvailabilityHistogramName + kOriginHistogramSuffix,
      FaviconAvailability::kNotAvailable, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetLocalImage) {
  mock_favicon_service_.StoreMockLocalFavicon(kPageUrl, kIconUrl);
  EXPECT_CALL(mock_favicon_service_, GetFaviconImageForPageURL(kPageUrl, _, _));
  EXPECT_CALL(mock_large_icon_service_, TouchIconFromGoogleServer(kIconUrl));
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      kPageUrl, base::BindOnce(&StoreImage, &result), kOrigin);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      kAvailabilityHistogramName + kOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetGoogleServerImage) {
  mock_large_icon_service_.StoreMockGoogleServerFavicon(kPageUrl, kIconUrl);
  EXPECT_CALL(can_send_history_data_getter_, Run());
  EXPECT_CALL(mock_favicon_service_, GetFaviconImageForPageURL(kPageUrl, _, _))
      .Times(2);
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  kPageUrl,
                  /*should_trim_page_url_path=*/false, _, _));
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      kPageUrl, base::BindOnce(&StoreImage, &result), kOrigin);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      kAvailabilityHistogramName + kOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest,
       ShouldNotQueryGoogleServerIfCannotSendData) {
  EXPECT_CALL(can_send_history_data_getter_, Run()).WillOnce([]() {
    return false;
  });
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(kPageUrl, _,
                                      /*desired_size_in_pixel=*/16, _, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kTaskId;
      });
  EXPECT_CALL(
      mock_large_icon_service_,
      GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(_, _, _, _))
      .Times(0);
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      kPageUrl, /*desired_size_in_pixel=*/16, base::DoNothing(), kOrigin);
}

}  // namespace
}  // namespace favicon
