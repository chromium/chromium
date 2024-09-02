// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/password_favicon_loader.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

namespace autofill {
namespace {

// TODO(crbug.com/325246516): Move to components/favicon/core/test.
class MockLargeIconService : public favicon::LargeIconService {
 public:
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
};

MATCHER_P(ImagesAreEqual, img, "") {
  // Compare bitmaps as `AreImagesEqual()` also compare the number of
  // representations which may be not equal.
  return gfx::test::AreBitmapsEqual(arg.AsBitmap(), img.AsBitmap());
}

favicon_base::LargeIconResult GetFaviconResult(const gfx::Image& image) {
  scoped_refptr<base::RefCountedBytes> data =
      base::MakeRefCounted<base::RefCountedBytes>();
  gfx::PNGCodec::EncodeBGRASkBitmap(
      /*input=*/*image.ToSkBitmap(),
      /*discard_transparency=*/false, /*output=*/&data->as_vector());
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.bitmap_data = data;
  return favicon_base::LargeIconResult(bitmap_result);
}

// `LargeIconService::GetLargeIconFromCacheFallbackToGoogleServer()` wrapper
// that simplifies its mocking by ignoring all arguments except the callback.
auto MockGetLargeIcon(
    testing::OnceAction<void(favicon_base::LargeIconCallback callback)>
        callback_handler) {
  return [callback_handler = std::move(callback_handler)](
             const GURL&, favicon::LargeIconService::StandardIconSize,
             std::optional<favicon::LargeIconService::StandardIconSize>,
             favicon::LargeIconService::NoBigEnoughIconBehavior, bool,
             const net::NetworkTrafficAnnotationTag&,
             favicon_base::LargeIconCallback result_callback,
             base::CancelableTaskTracker*) mutable {
    std::move(callback_handler).Call(std::move(result_callback));
  };
}

// `ImageFetcher::FetchImageAndData_()` wrapper that simplifies its mocking by
// ignoring all arguments except the callback.
auto MockFetchImageAndData(
    testing::OnceAction<void(image_fetcher::ImageFetcherCallback* callback)>
        callback_handler) {
  return [callback_handler = std::move(callback_handler)](
             const GURL& image_url, image_fetcher::ImageDataFetcherCallback*,
             image_fetcher::ImageFetcherCallback* callback,
             image_fetcher::ImageFetcherParams) mutable {
    std::move(callback_handler).Call(std::move(callback));
  };
}

class PasswordFaviconLoaderTest : public testing::Test {
 public:
  void SetUp() override {
    loader_ = std::make_unique<PasswordFaviconLoaderImpl>(&large_icon_service(),
                                                          &image_fetcher());
  }

 protected:
  MockLargeIconService& large_icon_service() {
    return mock_large_icon_service_;
  }

  image_fetcher::MockImageFetcher& image_fetcher() {
    return mock_image_fetcher_;
  }

  PasswordFaviconLoader& loader() { return *loader_; }

 private:
  MockLargeIconService mock_large_icon_service_;
  image_fetcher::MockImageFetcher mock_image_fetcher_;
  std::unique_ptr<PasswordFaviconLoaderImpl> loader_;
};

TEST_F(PasswordFaviconLoaderTest, LoadsImagesFromFaviconService) {
  EXPECT_CALL(large_icon_service(), GetLargeIconFromCacheFallbackToGoogleServer)
      .WillOnce(
          MockGetLargeIcon([](favicon_base::LargeIconCallback result_callback) {
            favicon_base::LargeIconResult result = GetFaviconResult(
                ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                    IDR_DISABLE));
            ASSERT_TRUE(result.bitmap.is_valid());
            std::move(result_callback).Run(result);
          }));

  base::MockOnceCallback<void(const gfx::Image&)> on_success;
  base::MockOnceClosure on_fail;

  EXPECT_CALL(
      on_success,
      Run(ImagesAreEqual(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_DISABLE))));
  EXPECT_CALL(on_fail, Run).Times(0);

  loader().Load(
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"),
                                 /*can_be_requested_from_google=*/true),
      /*task_tracker=*/nullptr, on_success.Get(), on_fail.Get());
}

TEST_F(PasswordFaviconLoaderTest, FailsWithInvalidResponseFromFaviconService) {
  EXPECT_CALL(large_icon_service(), GetLargeIconFromCacheFallbackToGoogleServer)
      .WillOnce(MockGetLargeIcon([](favicon_base::LargeIconCallback callback) {
        favicon_base::FaviconRawBitmapResult bitmap_result;
        ASSERT_FALSE(bitmap_result.is_valid());
        std::move(callback).Run(favicon_base::LargeIconResult(bitmap_result));
      }));

  base::MockOnceCallback<void(const gfx::Image&)> on_success;
  base::MockOnceClosure on_fail;

  EXPECT_CALL(on_success, Run).Times(0);
  EXPECT_CALL(on_fail, Run);

  loader().Load(
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"),
                                 /*can_be_requested_from_google=*/true),
      /*task_tracker=*/nullptr, on_success.Get(), on_fail.Get());
}

TEST_F(PasswordFaviconLoaderTest, LoadsImagesFromImageFetcher) {
  EXPECT_CALL(image_fetcher(), FetchImageAndData_)
      .WillOnce(MockFetchImageAndData(
          [](image_fetcher::ImageFetcherCallback* callback) {
            std::move(*callback).Run(
                ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                    IDR_DISABLE),
                image_fetcher::RequestMetadata());
          }));

  base::MockOnceCallback<void(const gfx::Image&)> on_success;
  base::MockOnceClosure on_fail;

  EXPECT_CALL(
      on_success,
      Run(ImagesAreEqual(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_DISABLE))));
  EXPECT_CALL(on_fail, Run).Times(0);

  loader().Load(
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"),
                                 /*can_be_requested_from_google=*/false),
      /*task_tracker=*/nullptr, on_success.Get(), on_fail.Get());
}

TEST_F(PasswordFaviconLoaderTest, FailsWithInvalidResponseFromImageFetcher) {
  EXPECT_CALL(image_fetcher(), FetchImageAndData_)
      .WillOnce(MockFetchImageAndData(
          [](image_fetcher::ImageFetcherCallback* callback) {
            std::move(*callback).Run(gfx::Image(),
                                     image_fetcher::RequestMetadata());
          }));

  base::MockOnceCallback<void(const gfx::Image&)> on_success;
  base::MockOnceClosure on_fail;

  EXPECT_CALL(on_success, Run).Times(0);
  EXPECT_CALL(on_fail, Run);

  loader().Load(
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"),
                                 /*can_be_requested_from_google=*/false),
      /*task_tracker=*/nullptr, on_success.Get(), on_fail.Get());
}

TEST_F(PasswordFaviconLoaderTest, ImagesFromFaviconServiceAreCached) {
  base::MockOnceCallback<void(const gfx::Image&)> on_success;
  base::MockOnceClosure on_fail;

  // Expect one call to the favicon service, after which the result will be
  // provided from cache.
  EXPECT_CALL(large_icon_service(), GetLargeIconFromCacheFallbackToGoogleServer)
      .WillOnce(MockGetLargeIcon([](favicon_base::LargeIconCallback callback) {
        favicon_base::LargeIconResult result = GetFaviconResult(
            ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_DISABLE));
        ASSERT_TRUE(result.bitmap.is_valid());
        std::move(callback).Run(result);
      }));
  EXPECT_CALL(
      on_success,
      Run(ImagesAreEqual(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_DISABLE))))
      .Times(2);
  EXPECT_CALL(on_fail, Run).Times(0);

  loader().Load(
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"),
                                 /*can_be_requested_from_google=*/true),
      /*task_tracker=*/nullptr, on_success.Get(), on_fail.Get());
  loader().Load(
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"),
                                 /*can_be_requested_from_google=*/true),
      /*task_tracker=*/nullptr, on_success.Get(), on_fail.Get());
}

TEST_F(PasswordFaviconLoaderTest, ImagesFromImageFetcherAreCached) {
  base::MockOnceCallback<void(const gfx::Image&)> on_success;
  base::MockOnceClosure on_fail;

  // Expect one call to the image fetcher, after which the result will be
  // provided from cache.
  EXPECT_CALL(image_fetcher(), FetchImageAndData_)
      .WillOnce(MockFetchImageAndData(
          [](image_fetcher::ImageFetcherCallback* callback) {
            std::move(*callback).Run(
                ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                    IDR_DISABLE),
                image_fetcher::RequestMetadata());
          }));
  EXPECT_CALL(
      on_success,
      Run(ImagesAreEqual(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_DISABLE))))
      .Times(2);
  EXPECT_CALL(on_fail, Run).Times(0);

  loader().Load(
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"),
                                 /*can_be_requested_from_google=*/false),
      /*task_tracker=*/nullptr, on_success.Get(), on_fail.Get());
  loader().Load(
      Suggestion::FaviconDetails(/*domain_url=*/GURL("https://google.com"),
                                 /*can_be_requested_from_google=*/false),
      /*task_tracker=*/nullptr, on_success.Get(), on_fail.Get());
}

}  // namespace
}  // namespace autofill
