// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_downloader.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

namespace {

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
  WebAppIconDownloaderTest(const WebAppIconDownloaderTest&) = delete;
  WebAppIconDownloaderTest& operator=(const WebAppIconDownloaderTest&) = delete;

  WebAppIconDownloaderTest() = default;
  ~WebAppIconDownloaderTest() override = default;

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

class WebAppIconDownloaderPrerenderTest : public WebAppIconDownloaderTest {
 public:
  WebAppIconDownloaderPrerenderTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        // This feature is to run test on any bot.
        {blink::features::kPrerender2MemoryControls});
  }
  ~WebAppIconDownloaderPrerenderTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

class TestWebAppIconDownloader : public WebAppIconDownloader {
 public:
  TestWebAppIconDownloader(content::WebContents* web_contents,
                           std::vector<GURL> extra_favicon_urls)
      : WebAppIconDownloader(
            web_contents,
            std::move(extra_favicon_urls),
            base::BindOnce(&TestWebAppIconDownloader::DownloadsComplete,
                           base::Unretained(this))) {}
  TestWebAppIconDownloader(const TestWebAppIconDownloader&) = delete;
  TestWebAppIconDownloader& operator=(const TestWebAppIconDownloader&) = delete;
  ~TestWebAppIconDownloader() override = default;

  int DownloadImage(const GURL& url) override { return id_counter_++; }

  const std::vector<blink::mojom::FaviconURLPtr>&
  GetFaviconURLsFromWebContents() override {
    return initial_favicon_urls_;
  }

  size_t pending_requests() const { return in_progress_requests_.size(); }

  void DownloadsComplete(IconsDownloadedResult result,
                         IconsMap icons_map,
                         DownloadedIconsHttpResults icons_http_results) {
    icons_download_result_ = result;
    icons_map_ = std::move(icons_map);
    icons_http_results_ = std::move(icons_http_results);

    run_loop_.Quit();
  }

  void AwaitDownloadsComplete() {
    run_loop_.Run();
    ASSERT_EQ(0u, pending_requests());
  }

  const IconsMap& icons_map() const { return icons_map_; }

  void CompleteImageDownload(
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<gfx::Size>& original_bitmap_sizes) {
    WebAppIconDownloader::DidDownloadFavicon(
        id, http_status_code, image_url,
        CreateTestBitmaps(original_bitmap_sizes), original_bitmap_sizes);
  }

  void UpdateFaviconURLs(
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
    WebAppIconDownloader::DidUpdateFaviconURL(web_contents()->GetMainFrame(),
                                              candidates);
  }

  void set_initial_favicon_urls(
      const std::vector<blink::mojom::FaviconURLPtr>& urls) {
    for (const auto& url : urls)
      initial_favicon_urls_.push_back(url.Clone());
  }

  IconsDownloadedResult icons_download_result() const {
    return icons_download_result_.value();
  }

  const DownloadedIconsHttpResults& icons_http_results() const {
    return icons_http_results_;
  }

 private:
  std::vector<blink::mojom::FaviconURLPtr> initial_favicon_urls_;

  IconsMap icons_map_;
  DownloadedIconsHttpResults icons_http_results_;

  int id_counter_ = 0;
  absl::optional<IconsDownloadedResult> icons_download_result_;
  base::RunLoop run_loop_;
};

TEST_F(WebAppIconDownloaderTest, SimpleDownload) {
  const GURL favicon_url("http://www.google.com/favicon.ico");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes(1, gfx::Size(32, 32));
  downloader.CompleteImageDownload(0, 200, favicon_urls[0]->icon_url, sizes);
  downloader.AwaitDownloadsComplete();

  EXPECT_EQ(1u, downloader.icons_map().size());
  EXPECT_EQ(1u, downloader.icons_map().at(favicon_url).size());

  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kCompleted);

  EXPECT_EQ(1u, downloader.icons_http_results().size());
  EXPECT_EQ(net::HttpStatusCode::HTTP_OK,
            downloader.icons_http_results().at(favicon_url));
}

TEST_F(WebAppIconDownloaderTest, NoHTTPStatusCode) {
  const GURL favicon_url("data:image/png,");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes = {gfx::Size(0, 0)};
  // data: URLs have a 0 HTTP status code.
  downloader.CompleteImageDownload(0, 0, favicon_urls[0]->icon_url, sizes);
  downloader.AwaitDownloadsComplete();

  EXPECT_EQ(1u, downloader.icons_map().size());
  EXPECT_EQ(1u, downloader.icons_map().at(favicon_url).size());

  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kCompleted)
      << "Should not consider data: URL or HTTP status code of 0 a failure";

  EXPECT_TRUE(downloader.icons_http_results().empty());
}

TEST_F(WebAppIconDownloaderTest, DownloadWithUrlsFromWebContentsNotification) {
  const GURL favicon_url("http://www.google.com/favicon.ico");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));
  EXPECT_EQ(0u, downloader.pending_requests());

  // Start downloader before favicon URLs are loaded.
  downloader.Start();
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.UpdateFaviconURLs(favicon_urls);
  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes(1, gfx::Size(32, 32));
  downloader.CompleteImageDownload(0, 200, favicon_urls[0]->icon_url, sizes);
  downloader.AwaitDownloadsComplete();

  EXPECT_EQ(1u, downloader.icons_map().size());
  EXPECT_EQ(1u, downloader.icons_map().at(favicon_url).size());

  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kCompleted);

  EXPECT_EQ(1u, downloader.icons_http_results().size());
  EXPECT_EQ(net::HttpStatusCode::HTTP_OK,
            downloader.icons_http_results().at(favicon_url));
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

  TestWebAppIconDownloader downloader(web_contents(), extra_urls);
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
  downloader.set_initial_favicon_urls(favicon_urls);
  downloader.Start();
  EXPECT_EQ(3u, downloader.pending_requests());

  std::vector<gfx::Size> sizes_1(1, gfx::Size(16, 16));
  downloader.CompleteImageDownload(0, 200, favicon_url_1, sizes_1);

  std::vector<gfx::Size> sizes_2;
  sizes_2.emplace_back(32, 32);
  sizes_2.emplace_back(64, 64);
  downloader.CompleteImageDownload(1, 200, favicon_url_2, sizes_2);

  // Only 1 download should have been initiated for |empty_favicon| even though
  // the URL was in both the web app info and the favicon urls.
  downloader.CompleteImageDownload(2, 200, empty_favicon,
                                   std::vector<gfx::Size>());
  downloader.AwaitDownloadsComplete();

  EXPECT_EQ(3u, downloader.icons_map().size());
  EXPECT_EQ(0u, downloader.icons_map().at(empty_favicon).size());
  EXPECT_EQ(1u, downloader.icons_map().at(favicon_url_1).size());
  EXPECT_EQ(2u, downloader.icons_map().at(favicon_url_2).size());

  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kCompleted);

  EXPECT_EQ(3u, downloader.icons_http_results().size());
  EXPECT_EQ(net::HttpStatusCode::HTTP_OK,
            downloader.icons_http_results().at(empty_favicon));
  EXPECT_EQ(net::HttpStatusCode::HTTP_OK,
            downloader.icons_http_results().at(favicon_url_1));
  EXPECT_EQ(net::HttpStatusCode::HTTP_OK,
            downloader.icons_http_results().at(favicon_url_2));
}

TEST_F(WebAppIconDownloaderTest, SkipPageFavicons) {
  const GURL favicon_url_1("http://www.google.com/favicon.ico");
  const GURL favicon_url_2("http://www.google.com/favicon2.ico");

  std::vector<GURL> extra_urls;
  extra_urls.push_back(favicon_url_1);

  TestWebAppIconDownloader downloader(web_contents(), extra_urls);

  // This favicon URL should be ignored.
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url_2, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  downloader.SkipPageFavicons();
  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes_1(1, gfx::Size(16, 16));
  downloader.CompleteImageDownload(0, 200, favicon_url_1, sizes_1);

  // This download should not be finished and inserted into the map.
  std::vector<gfx::Size> sizes_2;
  sizes_2.emplace_back(32, 32);
  sizes_2.emplace_back(64, 64);
  downloader.CompleteImageDownload(1, 200, favicon_url_2, sizes_2);
  downloader.AwaitDownloadsComplete();

  EXPECT_EQ(1u, downloader.icons_map().size());
  EXPECT_EQ(1u, downloader.icons_map().at(favicon_url_1).size());
  EXPECT_EQ(0u, downloader.icons_map().count(favicon_url_2));

  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kCompleted);

  EXPECT_EQ(1u, downloader.icons_http_results().size());
  EXPECT_EQ(net::HttpStatusCode::HTTP_OK,
            downloader.icons_http_results().at(favicon_url_1));
}

TEST_F(WebAppIconDownloaderTest, PageNavigates) {
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL("http://www.google.com/favicon.ico"),
      blink::mojom::FaviconIconType::kFavicon, std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://foo.example"), main_rfh())
      ->Commit();

  EXPECT_EQ(0u, downloader.pending_requests());
  EXPECT_TRUE(downloader.icons_map().empty());
  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kPrimaryPageChanged);
}

TEST_F(WebAppIconDownloaderTest, PageNavigatesAfterDownload) {
  const GURL url("http://www.google.com/icon.png");
  TestWebAppIconDownloader downloader(web_contents(), {url});
  downloader.SkipPageFavicons();

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  downloader.CompleteImageDownload(0, 200, url, {gfx::Size(32, 32)});
  downloader.AwaitDownloadsComplete();
  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kCompleted);

  // Navigating the renderer after downloads have completed should not crash.
  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://foo.example"), main_rfh())
      ->Commit();
}

TEST_F(WebAppIconDownloaderTest, PageNavigatesSameDocument) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://foo.example"));

  const GURL favicon_url("http://www.google.com/favicon.ico");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));

  downloader.set_initial_favicon_urls(favicon_urls);
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://foo.example/#test"), main_rfh())
      ->CommitSameDocument();

  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes(1, gfx::Size(32, 32));
  downloader.CompleteImageDownload(0, 200, favicon_url, sizes);
  downloader.AwaitDownloadsComplete();

  EXPECT_EQ(1u, downloader.icons_map().size());
  EXPECT_EQ(1u, downloader.icons_map().at(favicon_url).size());

  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kCompleted);

  EXPECT_EQ(1u, downloader.icons_http_results().size());
  EXPECT_EQ(net::HttpStatusCode::HTTP_OK,
            downloader.icons_http_results().at(favicon_url));
}

TEST_F(WebAppIconDownloaderPrerenderTest, PrerenderedPageNavigates) {
  // Navigate to an initial page.
  NavigateAndCommit(GURL("http://foo.example"));

  const GURL favicon_url("http://www.google.com/favicon.ico");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      favicon_url, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

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
      prerender_url, web_contents()->GetMainFrame())
      ->Commit();

  // Ensure prerender activation cancel pending download requests.
  EXPECT_EQ(0u, downloader.pending_requests());
  EXPECT_TRUE(downloader.icons_map().empty());
  EXPECT_EQ(downloader.icons_download_result(),
            IconsDownloadedResult::kPrimaryPageChanged);
}

}  // namespace web_app
