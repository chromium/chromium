// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_icon_fetcher.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "components/favicon/content/large_icon_service_getter.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_page_data.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace webapps {

namespace {

const char* kPageUrl = "https://www.example.com";

SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

favicon_base::FaviconRawBitmapResult CreateFaviconRawBitmapResult(
    const SkBitmap& bitmap,
    const GURL& icon_url) {
  favicon_base::FaviconRawBitmapResult result;
  if (!bitmap.isNull()) {
    std::optional<std::vector<uint8_t>> png_data =
        gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                          /*discard_transparency=*/false);
    if (png_data) {
      result.bitmap_data =
          base::MakeRefCounted<base::RefCountedBytes>(std::move(*png_data));
      result.pixel_size = gfx::Size(bitmap.width(), bitmap.height());
      result.icon_url = icon_url;
      result.icon_type = favicon_base::IconType::kFavicon;
    }
  }
  return result;
}

class TestLargeIconService : public favicon::LargeIconService {
 public:
  TestLargeIconService() = default;
  ~TestLargeIconService() override = default;

  void SetRawBitmapResult(const favicon_base::FaviconRawBitmapResult& result) {
    raw_bitmap_result_ = result;
  }

  base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override {
    return base::CancelableTaskTracker::kBadTaskId;
  }

  base::CancelableTaskTracker::TaskId
  GetLargeIconImageOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconImageCallback callback,
      base::CancelableTaskTracker* tracker) override {
    return base::CancelableTaskTracker::kBadTaskId;
  }

  base::CancelableTaskTracker::TaskId GetLargeIconRawBitmapForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      std::optional<int> size_in_pixel_to_resize_to,
      NoBigEnoughIconBehavior no_big_enough_icon_behavior,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       favicon_base::LargeIconResult(raw_bitmap_result_)));
    return 1;
  }

  base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      const GURL& icon_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override {
    return base::CancelableTaskTracker::kBadTaskId;
  }

  base::CancelableTaskTracker::TaskId GetIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override {
    return base::CancelableTaskTracker::kBadTaskId;
  }

  void GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
      const GURL& page_url,
      bool should_trim_page_url_path,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      favicon_base::GoogleFaviconServerCallback callback) override {}

  void GetLargeIconFromCacheFallbackToGoogleServer(
      const GURL& page_url,
      StandardIconSize min_source_size,
      std::optional<StandardIconSize> size_to_resize_to,
      NoBigEnoughIconBehavior no_big_enough_icon_behavior,
      bool should_trim_page_url_path,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override {}

  void TouchIconFromGoogleServer(const GURL& icon_url) override {}

 private:
  favicon_base::FaviconRawBitmapResult raw_bitmap_result_;
};

}  // namespace

class InstallableIconFetcherTest : public content::RenderViewHostTestHarness {
 public:
  InstallableIconFetcherTest() = default;
  ~InstallableIconFetcherTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    scoped_ideal_favicon_size_.emplace(&test::g_ideal_favicon_size_for_testing,
                                       144);
    favicon::SetLargeIconServiceGetter(
        base::BindRepeating(&InstallableIconFetcherTest::GetLargeIconService,
                            base::Unretained(this)));
    web_contents_tester()->NavigateAndCommit(GURL(kPageUrl));
  }

  void TearDown() override {
    scoped_ideal_favicon_size_.reset();
    favicon::SetLargeIconServiceGetter(favicon::LargeIconServiceGetter());
    content::RenderViewHostTestHarness::TearDown();
  }

  favicon::LargeIconService* GetLargeIconService(content::BrowserContext*) {
    return large_icon_service_.get();
  }

  void EnableLargeIconService() {
    large_icon_service_ = std::make_unique<TestLargeIconService>();
  }

  TestLargeIconService* large_icon_service() {
    return large_icon_service_.get();
  }

  void SimulateFaviconCandidateDownloaded(InstallableIconFetcher& fetcher,
                                          const GURL& icon_url,
                                          const SkBitmap& bitmap) {
    fetcher.OnFaviconCandidateDownloaded(icon_url, bitmap);
  }

 protected:
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

 private:
  std::optional<base::AutoReset<int>> scoped_ideal_favicon_size_;
  std::unique_ptr<TestLargeIconService> large_icon_service_;
};

TEST_F(InstallableIconFetcherTest, FaviconFallbackDownloadsTouchIcon) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kTouchIconUrl("https://www.example.com/touch-icon.png");
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kTouchIconUrl, blink::mojom::FaviconIconType::kTouchIcon,
      std::vector<gfx::Size>(), /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kTouchIconUrl));
  SkBitmap test_bitmap = CreateTestBitmap(180, 180);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kTouchIconUrl, 200, {test_bitmap}, {gfx::Size(180, 180)}));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kTouchIconUrl);
  ASSERT_TRUE(page_data.primary_icon());
  EXPECT_FALSE(page_data.primary_icon()->drawsNothing());
  EXPECT_GT(page_data.primary_icon()->width(), 0);
  EXPECT_GT(page_data.primary_icon()->height(), 0);
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackSkipsSmallIconAndDownloadsLargeCandidate) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kSmallIconUrl("https://www.example.com/small.png");
  const GURL kLargeIconUrl("https://www.example.com/large.png");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kSmallIconUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(16, 16), gfx::Size(32, 32)},
      /*is_default_icon=*/false));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kLargeIconUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(192, 192)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // The small icon is filtered out because all its specified sizes are < 48px.
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kSmallIconUrl));
  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kLargeIconUrl));

  SkBitmap test_bitmap = CreateTestBitmap(192, 192);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kLargeIconUrl, 200, {test_bitmap}, {gfx::Size(192, 192)}));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kLargeIconUrl);
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackCandidateDownloadFailureEndsWithError) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kFirstCandidateUrl("https://www.example.com/first.png");
  const GURL kSecondCandidateUrl("https://www.example.com/second.png");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kFirstCandidateUrl, blink::mojom::FaviconIconType::kTouchIcon,
      std::vector<gfx::Size>(), /*is_default_icon=*/false));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kSecondCandidateUrl, blink::mojom::FaviconIconType::kTouchIcon,
      std::vector<gfx::Size>(), /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  EXPECT_TRUE(
      web_contents_tester()->HasPendingDownloadImage(kFirstCandidateUrl));
  // Simulate first download failed (404 / empty bitmaps).
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(kFirstCandidateUrl,
                                                          404, {}, {}));

  // With fallback abandoned, the second candidate should not be downloaded and
  // the fetch should end with NO_ACCEPTABLE_ICON immediately.
  EXPECT_FALSE(
      web_contents_tester()->HasPendingDownloadImage(kSecondCandidateUrl));
  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ACCEPTABLE_ICON);
  EXPECT_FALSE(page_data.primary_icon());
}

TEST_F(InstallableIconFetcherTest, FaviconFallbackHandlesAllCandidatesFailing) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kFailingCandidateUrl("https://www.example.com/failing.png");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kFailingCandidateUrl, blink::mojom::FaviconIconType::kTouchIcon,
      std::vector<gfx::Size>(), /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  EXPECT_TRUE(
      web_contents_tester()->HasPendingDownloadImage(kFailingCandidateUrl));
  // Download returns empty bitmap.
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(kFailingCandidateUrl,
                                                          200, {}, {}));

#if BUILDFLAG(IS_DESKTOP_ANDROID)
  // On Desktop Android, MaybeEndWithError generates a monogram homescreen icon.
  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_TRUE(page_data.primary_icon());
#else
  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ACCEPTABLE_ICON);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.icon_error(), InstallableStatusCode::NO_ACCEPTABLE_ICON);
  EXPECT_FALSE(page_data.primary_icon());
#endif
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackWhenLargeIconServiceReturnsSmallIcon) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  EnableLargeIconService();
  // Simulate LargeIconService returning a 16x16 icon (too small).
  const GURL kSmallDbIconUrl("https://www.example.com/favicon-16.png");
  SkBitmap small_db_bitmap = CreateTestBitmap(16, 16);
  large_icon_service()->SetRawBitmapResult(
      CreateFaviconRawBitmapResult(small_db_bitmap, kSmallDbIconUrl));

  // The page has a 180x180 touch icon candidate in the DOM.
  const GURL kTouchIconUrl("https://www.example.com/touch-icon.png");
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kTouchIconUrl, blink::mojom::FaviconIconType::kTouchIcon,
      std::vector<gfx::Size>(), /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // Wait for LargeIconService and background processing to complete and fall
  // back to candidate fetching.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return web_contents_tester()->HasPendingDownloadImage(kTouchIconUrl);
  }));

  SkBitmap test_bitmap = CreateTestBitmap(180, 180);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kTouchIconUrl, 200, {test_bitmap}, {gfx::Size(180, 180)}));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kTouchIconUrl);
  ASSERT_TRUE(page_data.primary_icon());
  EXPECT_FALSE(page_data.primary_icon()->drawsNothing());
  EXPECT_GT(page_data.primary_icon()->width(), 0);
  EXPECT_GT(page_data.primary_icon()->height(), 0);
}

TEST_F(InstallableIconFetcherTest, FaviconServiceLargeIconUsedDirectly) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  EnableLargeIconService();
  // Simulate LargeIconService returning a 192x192 icon (>= 48px).
  const GURL kLargeDbIconUrl("https://www.example.com/favicon-192.png");
  SkBitmap large_db_bitmap = CreateTestBitmap(192, 192);
  large_icon_service()->SetRawBitmapResult(
      CreateFaviconRawBitmapResult(large_db_bitmap, kLargeDbIconUrl));

  // The page also has a candidate in the DOM.
  const GURL kCandidateUrl("https://www.example.com/touch-icon.png");
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidateUrl, blink::mojom::FaviconIconType::kTouchIcon,
      std::vector<gfx::Size>(), /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // The LargeIconService icon was sufficient, so it shouldn't download
  // candidates. future.Get() automatically waits for the fetch to complete.
  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kCandidateUrl));
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kLargeDbIconUrl);
  ASSERT_TRUE(page_data.primary_icon());
  EXPECT_FALSE(page_data.primary_icon()->drawsNothing());
  EXPECT_GT(page_data.primary_icon()->width(), 0);
  EXPECT_GT(page_data.primary_icon()->height(), 0);
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackWhenMinSizeGreaterThanIdealSize) {
  // Set min size to 1000px, which exceeds the ideal size (144px).
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 1000);

  const GURL kCandidateUrl("https://www.example.com/icon.png");
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidateUrl, blink::mojom::FaviconIconType::kTouchIcon,
      std::vector<gfx::Size>{gfx::Size(1024, 1024)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // Verify that ManifestIconDownloader does not trigger DCHECK failure
  // (minimum_icon_size_in_px <= ideal_icon_size_in_px).
  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kCandidateUrl));
  SkBitmap bitmap = CreateTestBitmap(1024, 1024);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kCandidateUrl, 200, {bitmap}, {gfx::Size(1024, 1024)}));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kCandidateUrl);
}

TEST_F(InstallableIconFetcherTest,
       WebContentsDestroyedBeforeCandidateDownloading) {
  EnableLargeIconService();

  const GURL kCandidateUrl("https://www.example.com/icon.png");
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidateUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(192, 192)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // Destroy WebContents while LargeIconService query is in flight (before
  // FetchFaviconFromCandidates runs).
  DeleteContents();

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ACCEPTABLE_ICON);
  EXPECT_FALSE(page_data.primary_icon());
}

TEST_F(InstallableIconFetcherTest,
       WebContentsDestroyedDuringCandidateDownloading) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kCandidateUrl1("https://www.example.com/icon1.png");
  const GURL kCandidateUrl2("https://www.example.com/icon2.png");
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidateUrl1, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(192, 192)},
      /*is_default_icon=*/false));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidateUrl2, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(192, 192)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kCandidateUrl1));

  // Destroy WebContents while candidate 1 download is in flight.
  DeleteContents();

  // Simulate candidate 1 download failing after WebContents destruction.
  SimulateFaviconCandidateDownloaded(fetcher, kCandidateUrl1, SkBitmap());

  // DownloadNextFaviconCandidate should notice !web_contents_ and end with
  // NO_ACCEPTABLE_ICON without crashing MaybeEndWithError.
  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ACCEPTABLE_ICON);
  EXPECT_FALSE(page_data.primary_icon());
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackDuplicateUrlPrioritizesLargerSize) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kDuplicateUrl("https://www.example.com/shared-favicon.png");
  const GURL kOtherUrl("https://www.example.com/other-icon.png");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  // First, declare kDuplicateUrl with a small size (< 48px).
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kDuplicateUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(16, 16)},
      /*is_default_icon=*/false));
  // Then an intermediate candidate with 96x96.
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kOtherUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(96, 96)},
      /*is_default_icon=*/false));
  // Finally, declare kDuplicateUrl with a large size (192x192).
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kDuplicateUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(192, 192)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // kDuplicateUrl should be chosen first because its 192px declaration ranks
  // higher than kOtherUrl (96px), and the earlier 16px entry does not discard
  // it.
  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kDuplicateUrl));
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kOtherUrl));

  SkBitmap test_bitmap = CreateTestBitmap(192, 192);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kDuplicateUrl, 200, {test_bitmap}, {gfx::Size(192, 192)}));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kDuplicateUrl);
}

TEST_F(InstallableIconFetcherTest, FaviconFallbackCandidateSizesAny) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kSmallIconUrl("https://www.example.com/small.png");
  const GURL kSvgIconUrl("https://www.example.com/icon.svg");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kSmallIconUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(16, 16)},
      /*is_default_icon=*/false));
  // sizes="any" is represented as an empty gfx::Size in Blink icon_sizes.
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kSvgIconUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size()},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // The 16px icon is skipped, while the SVG (sizes="any") candidate is
  // downloaded.
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kSmallIconUrl));
  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kSvgIconUrl));

  SkBitmap test_bitmap = CreateTestBitmap(192, 192);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kSvgIconUrl, 200, {test_bitmap}, {gfx::Size(192, 192)}));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kSvgIconUrl);
  ASSERT_TRUE(page_data.primary_icon());
  EXPECT_FALSE(page_data.primary_icon()->drawsNothing());
  EXPECT_GT(page_data.primary_icon()->width(), 0);
  EXPECT_GT(page_data.primary_icon()->height(), 0);
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackSelectsFirstBigEnoughCandidateInDocumentOrder) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kCandidate192("https://www.example.com/icon-192.png");
  const GURL kCandidate512("https://www.example.com/icon-512.png");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  // In document order, 192x192 appears before 512x512. Both are >= ideal_size
  // (144px).
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidate192, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(192, 192)},
      /*is_default_icon=*/false));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidate512, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(512, 512)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // The first big-enough candidate (192px) should be selected first in
  // document order, rather than the largest candidate (512px).
  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kCandidate192));
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kCandidate512));

  SkBitmap bitmap192 = CreateTestBitmap(192, 192);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kCandidate192, 200, {bitmap192}, {gfx::Size(192, 192)}));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kCandidate192);
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kCandidate512));
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackFirstCandidateFailureDoesNotFallback) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kCandidate192("https://www.example.com/icon-192.png");
  const GURL kCandidate512("https://www.example.com/icon-512.png");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidate192, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(192, 192)},
      /*is_default_icon=*/false));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidate512, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(512, 512)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kCandidate192));
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kCandidate512));

  // If the 192px candidate fails to download (e.g. 404), it should error
  // immediately without falling back to 512px.
  EXPECT_TRUE(
      web_contents_tester()->TestDidDownloadImage(kCandidate192, 404, {}, {}));
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kCandidate512));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ACCEPTABLE_ICON);
  EXPECT_FALSE(page_data.primary_icon());
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackPrefersSvgCandidateInDocumentOrder) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kSvgUrl("https://www.example.com/vector-icon.svg");
  const GURL kPngUrl("https://www.example.com/raster-icon.png");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  // SVG candidate appears first without explicit sizes.
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kSvgUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>(), /*is_default_icon=*/false));
  // 192x192 PNG candidate appears second.
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kPngUrl, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(192, 192)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // SVG is "big enough" and should be selected first in document order.
  EXPECT_TRUE(web_contents_tester()->HasPendingDownloadImage(kSvgUrl));
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kPngUrl));

  SkBitmap bitmap = CreateTestBitmap(192, 192);
  EXPECT_TRUE(web_contents_tester()->TestDidDownloadImage(
      kSvgUrl, 200, {bitmap}, {gfx::Size(192, 192)}));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ERROR_DETECTED);
  EXPECT_TRUE(page_data.primary_icon_fetched());
  EXPECT_EQ(page_data.primary_icon_url(), kSvgUrl);
}

TEST_F(InstallableIconFetcherTest,
       FaviconFallbackErrorsWhenNoCandidateIsBigEnough) {
  base::AutoReset<int> scoped_min_favicon_size(
      &test::g_minimum_favicon_size_for_testing, 48);

  const GURL kCandidate48("https://www.example.com/icon-48.png");
  const GURL kCandidate96("https://www.example.com/icon-96.png");

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  // Neither 48px nor 96px is "big enough" (ideal_size is 144px).
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidate48, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(48, 48)},
      /*is_default_icon=*/false));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      kCandidate96, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(96, 96)},
      /*is_default_icon=*/false));
  web_contents_tester()->TestSetFaviconURL(std::move(favicon_urls));

  base::test::TestFuture<InstallableStatusCode> future;
  InstallablePageData page_data;
  std::vector<blink::Manifest::ImageResource> manifest_icons;
  InstallableIconFetcher fetcher(web_contents(), page_data, manifest_icons,
                                 /*prefer_maskable=*/false,
                                 /*fetch_favicon=*/true, future.GetCallback());

  // Since neither is big enough, no download should be attempted and the fetch
  // should end with NO_ACCEPTABLE_ICON immediately.
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kCandidate96));
  EXPECT_FALSE(web_contents_tester()->HasPendingDownloadImage(kCandidate48));

  EXPECT_EQ(future.Get(), InstallableStatusCode::NO_ACCEPTABLE_ICON);
  EXPECT_FALSE(page_data.primary_icon());
}

}  // namespace webapps
