// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"

#include <stddef.h>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {
namespace {

using ::testing::_;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::IsTrue;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// Creates valid SkBitmaps of the dimensions found in |sizes| and pushes them
// into |bitmaps|.
std::vector<SkBitmap> CreateTestBitmaps(const std::vector<gfx::Size>& sizes) {
  std::vector<SkBitmap> bitmaps(sizes.size());
  for (size_t i = 0; i < sizes.size(); ++i) {
    SkBitmap& bitmap = bitmaps[i];
    bitmap.allocN32Pixels(sizes[i].width(), sizes[i].height());
    bitmap.eraseColor(SK_ColorRED);
  }
  return bitmaps;
}

class WebAppIconDownloaderTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    // Specifies HTTPS for web_contents()->GetLastCommittedURL().
    web_contents_tester()->NavigateAndCommit(GURL("https://www.example.com"));
  }

 protected:
  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }
};

TEST_F(WebAppIconDownloaderTest, SimpleDownload) {
  const auto favicon_url = blink::mojom::FaviconURL::New(
      GURL{"http://www.google.com/favicon.ico"},
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>());

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(mojo::Clone(favicon_url));

  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));

  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;

  downloader.Start(web_contents(), std::vector<GURL>(),
                   test_future.GetCallback());
  const std::vector<gfx::Size> sizes{gfx::Size(32, 32)};
  web_contents_tester()->TestDidDownloadImage(
      /*url=*/favicon_url->icon_url,
      /*http_status_code=*/200,
      /*bitmaps=*/CreateTestBitmaps(sizes),
      /*original_bitmap_sizes=*/sizes);

  EXPECT_THAT(
      test_future.Get(),
      FieldsAre(
          /*result=*/IconsDownloadedResult::kCompleted,
          /*icons_map=*/UnorderedElementsAre(Pair(favicon_url->icon_url, _)),
          /*icons_http_results=*/
          UnorderedElementsAre(
              Pair(favicon_url->icon_url, net::HttpStatusCode::HTTP_OK))));
}

TEST_F(WebAppIconDownloaderTest, NoHTTPStatusCode) {
  const auto favicon_url = blink::mojom::FaviconURL::New(
      GURL{"data:image/png,"}, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>());

  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(mojo::Clone(favicon_url));
  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));

  downloader.Start(web_contents(), std::vector<GURL>(),
                   test_future.GetCallback());

  std::vector<gfx::Size> sizes = {gfx::Size(0, 0)};
  // data: URLs have a 0 HTTP status code.
  web_contents_tester()->TestDidDownloadImage(
      /*url=*/favicon_url->icon_url,
      /*http_status_code=*/0,
      /*bitmaps=*/CreateTestBitmaps(sizes),
      /*original_bitmap_sizes=*/sizes);

  EXPECT_THAT(
      test_future.Get(),
      FieldsAre(
          /*result=*/IconsDownloadedResult::kCompleted,
          /*icons_map=*/UnorderedElementsAre(Pair(favicon_url->icon_url, _)),
          /*icons_http_results=*/IsEmpty()));
}

TEST_F(WebAppIconDownloaderTest, DownloadMultipleUrls) {
  const GURL empty_favicon("http://www.google.com/empty_favicon.ico");
  const GURL favicon_url_1("http://www.google.com/favicon.ico");
  const GURL favicon_url_2("http://www.google.com/favicon2.ico");

  std::vector<GURL> extra_urls;
  // This should get downloaded.
  extra_urls.push_back(favicon_url_2);
  // This is duplicated in the favicon urls and should only be downloaded once.
  extra_urls.push_back(empty_favicon);

  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url_1, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));
  // This is duplicated in the favicon urls and should only be downloaded once.
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      empty_favicon, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));
  // Invalid icons shouldn't get put into the download queue.
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL("http://www.google.com/invalid.ico"),
      blink::mojom::FaviconIconType::kInvalid, std::vector<gfx::Size>()));

  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));

  downloader.Start(web_contents(), extra_urls, test_future.GetCallback());

  std::vector<gfx::Size> sizes_1(1, gfx::Size(16, 16));
  web_contents_tester()->TestDidDownloadImage(
      /*url=*/favicon_url_1,
      /*http_status_code=*/200,
      /*bitmaps=*/CreateTestBitmaps(sizes_1),
      /*original_bitmap_sizes=*/sizes_1);

  std::vector<gfx::Size> sizes_2;
  sizes_2.emplace_back(32, 32);
  sizes_2.emplace_back(64, 64);
  web_contents_tester()->TestDidDownloadImage(
      /*url=*/favicon_url_2,
      /*http_status_code=*/200,
      /*bitmaps=*/CreateTestBitmaps(sizes_2),
      /*original_bitmap_sizes=*/sizes_2);

  // Only 1 download should have been initiated for |empty_favicon| even though
  // the URL was in both the web app info and the favicon urls.
  web_contents_tester()->TestDidDownloadImage(
      /*url=*/empty_favicon,
      /*http_status_code=*/200,
      /*bitmaps=*/{},
      /*original_bitmap_sizes=*/{});

  EXPECT_THAT(
      test_future.Get(),
      FieldsAre(
          /*result=*/IconsDownloadedResult::kCompleted,
          /*icons_map=*/
          UnorderedElementsAre(Pair(empty_favicon, _), Pair(favicon_url_1, _),
                               Pair(favicon_url_2, _)),
          /*icons_http_results=*/
          UnorderedElementsAre(
              Pair(empty_favicon, net::HttpStatusCode::HTTP_OK),
              Pair(favicon_url_1, net::HttpStatusCode::HTTP_OK),
              Pair(favicon_url_2, net::HttpStatusCode::HTTP_OK))));
}

TEST_F(WebAppIconDownloaderTest, SkipPageFavicons) {
  const GURL favicon_url_1("http://www.google.com/favicon.ico");
  const GURL favicon_url_2("http://www.google.com/favicon2.ico");

  std::vector<GURL> extra_urls;
  extra_urls.push_back(favicon_url_1);

  // This favicon URL should be ignored.
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url_2, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));

  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));

  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;
  downloader.Start(web_contents(), extra_urls, test_future.GetCallback(),
                   {.skip_page_favicons = true});

  const std::vector<gfx::Size> sizes_1 = {gfx::Size{16, 16}};
  web_contents_tester()->TestDidDownloadImage(
      /*url=*/favicon_url_1,
      /*http_status_code=*/200,
      /*bitmaps=*/CreateTestBitmaps(sizes_1),
      /*original_bitmap_sizes=*/sizes_1);

  // This download should not be finished and inserted into the map.
  const std::vector<gfx::Size> sizes_2 = {
      gfx::Size{32, 32},
      gfx::Size{64, 64},
  };

  web_contents_tester()->TestDidDownloadImage(
      /*url=*/favicon_url_2,
      /*http_status_code=*/200,
      /*bitmaps=*/CreateTestBitmaps(sizes_2),
      /*original_bitmap_sizes=*/sizes_2);

  EXPECT_THAT(test_future.Get(),
              FieldsAre(
                  /*result=*/IconsDownloadedResult::kCompleted,
                  /*icons_map=*/
                  UnorderedElementsAre(Pair(favicon_url_1, _)),
                  /*icons_http_results=*/
                  UnorderedElementsAre(
                      Pair(favicon_url_1, net::HttpStatusCode::HTTP_OK))));
}

TEST_F(WebAppIconDownloaderTest, ShuttingDown) {
  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL("http://www.google.com/favicon.ico"),
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>()));
  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));

  downloader.Start(web_contents(), std::vector<GURL>(),
                   test_future.GetCallback());

  static_cast<content::WebContentsObserver&>(downloader).WebContentsDestroyed();

  EXPECT_THAT(test_future.Get(),
              FieldsAre(
                  /*result=*/IconsDownloadedResult::kPrimaryPageChanged,
                  /*icons_map=*/IsEmpty(),
                  /*icons_http_results=*/_));
}

TEST_F(WebAppIconDownloaderTest, PageNavigates) {
  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL("http://www.google.com/favicon.ico"),
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>()));
  web_contents_tester()->TestSetFaviconURL(mojo::Clone(favicon_urls));

  downloader.Start(web_contents(), std::vector<GURL>(),
                   test_future.GetCallback());

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://foo.example"), main_rfh())
      ->Commit();

  EXPECT_THAT(test_future.Get(),
              FieldsAre(
                  /*result=*/IconsDownloadedResult::kPrimaryPageChanged,
                  /*icons_map=*/IsEmpty(),
                  /*icons_http_results=*/_));
}

TEST_F(WebAppIconDownloaderTest, PageNavigatesAfterDownload) {
  const GURL url("http://www.google.com/icon.png");

  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;
  downloader.Start(web_contents(), std::vector<GURL>{url},
                   test_future.GetCallback(), {.skip_page_favicons = true});

  std::vector<gfx::Size> sizes = {gfx::Size(32, 32)};
  web_contents_tester()->TestDidDownloadImage(
      /*url=*/url,
      /*http_status_code=*/200,
      /*bitmaps=*/CreateTestBitmaps(sizes),
      /*original_bitmap_sizes=*/sizes);

  EXPECT_THAT(test_future.Get(),
              FieldsAre(
                  /*result=*/IconsDownloadedResult::kCompleted,
                  /*icons_map=*/_,
                  /*icons_http_results=*/_));

  // Navigating the renderer after downloads have completed should not crash.
  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://foo.example"), main_rfh())
      ->Commit();
}

TEST_F(WebAppIconDownloaderTest, PageNavigatesSameDocument) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://foo.example"));

  const GURL favicon_url("http://www.google.com/favicon.ico");
  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));

  web_contents_tester()->TestUpdateFaviconURL(mojo::Clone(favicon_urls));

  downloader.Start(web_contents(), std::vector<GURL>(),
                   test_future.GetCallback());

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://foo.example/#test"), main_rfh())
      ->CommitSameDocument();

  std::vector<gfx::Size> sizes(1, gfx::Size(32, 32));
  web_contents_tester()->TestDidDownloadImage(
      /*url=*/favicon_url,
      /*http_status_code=*/200,
      /*bitmaps=*/CreateTestBitmaps(sizes),
      /*original_bitmap_sizes=*/sizes);

  EXPECT_THAT(test_future.Get(),
              FieldsAre(
                  /*result=*/IconsDownloadedResult::kCompleted,
                  /*icons_map=*/UnorderedElementsAre(Pair(favicon_url, _)),
                  /*icons_http_results=*/
                  UnorderedElementsAre(
                      Pair(favicon_url, net::HttpStatusCode::HTTP_OK))));
}

class WebAppIconDownloaderPrerenderTest : public WebAppIconDownloaderTest {
 public:
  WebAppIconDownloaderPrerenderTest() = default;

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

TEST_F(WebAppIconDownloaderPrerenderTest, PrerenderedPageNavigates) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  // Navigate to an initial page.
  NavigateAndCommit(GURL("http://foo.example"));

  const GURL favicon_url("http://www.google.com/favicon.ico");
  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      test_future;
  WebAppIconDownloader downloader;
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));

  web_contents_tester()->TestUpdateFaviconURL(mojo::Clone(favicon_urls));
  downloader.Start(web_contents(), std::vector<GURL>(),
                   test_future.GetCallback());

  // Start a prerender and navigate the test page.
  const GURL& prerender_url = GURL("http://foo.example/bar");
  content::RenderFrameHost* prerender_frame =
      content::WebContentsTester::For(web_contents())
          ->AddPrerenderAndCommitNavigation(prerender_url);
  ASSERT_EQ(prerender_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kPrerendering);

  // Ensure prerender navigation doesn't cancel pending download requests.
  EXPECT_EQ(1u, downloader.pending_requests());

  // Activate the prerendered page.
  content::NavigationSimulator::CreateRendererInitiated(
      prerender_url, web_contents()->GetPrimaryMainFrame())
      ->Commit();

  // Ensure prerender activation cancel pending download requests.
  EXPECT_EQ(0u, downloader.pending_requests());

  EXPECT_THAT(test_future.Get(),
              FieldsAre(
                  /*result=*/IconsDownloadedResult::kPrimaryPageChanged,
                  /*icons_map=*/IsEmpty(),
                  /*icons_http_results=*/_));
}

}  // namespace
}  // namespace web_app
