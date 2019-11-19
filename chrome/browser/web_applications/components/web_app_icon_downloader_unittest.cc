// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/favicon_url.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::RenderViewHostTester;

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

class WebAppIconDownloaderTest : public ChromeRenderViewHostTestHarness {
 protected:
  WebAppIconDownloaderTest() {}

  ~WebAppIconDownloaderTest() override {}

 protected:
  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppIconDownloaderTest);
};

const char* kHistogramForCreateName = "WebApp.Icon.HttpStatusCodeClassOnCreate";

}  // namespace

class TestWebAppIconDownloader : public WebAppIconDownloader {
 public:
  TestWebAppIconDownloader(content::WebContents* web_contents,
                           std::vector<GURL> extra_favicon_urls)
      : WebAppIconDownloader(
            web_contents,
            extra_favicon_urls,
            Histogram::kForCreate,
            base::BindOnce(&TestWebAppIconDownloader::DownloadsComplete,
                           base::Unretained(this))),
        id_counter_(0) {}
  ~TestWebAppIconDownloader() override {}

  int DownloadImage(const GURL& url) override { return id_counter_++; }

  std::vector<content::FaviconURL> GetFaviconURLsFromWebContents() override {
    return initial_favicon_urls_;
  }

  size_t pending_requests() const { return in_progress_requests_.size(); }

  void DownloadsComplete(bool success, IconsMap map) {
    downloads_succeeded_ = success;
    favicon_map_ = std::move(map);
  }

  IconsMap favicon_map() const { return favicon_map_; }

  void CompleteImageDownload(
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<gfx::Size>& original_bitmap_sizes) {
    WebAppIconDownloader::DidDownloadFavicon(
        id, http_status_code, image_url,
        CreateTestBitmaps(original_bitmap_sizes), original_bitmap_sizes);
  }

  void UpdateFaviconURLs(const std::vector<content::FaviconURL>& candidates) {
    WebAppIconDownloader::DidUpdateFaviconURL(candidates);
  }

  void set_initial_favicon_urls(const std::vector<content::FaviconURL>& urls) {
    initial_favicon_urls_ = urls;
  }

  bool downloads_succeeded() { return downloads_succeeded_.value(); }

 private:
  std::vector<content::FaviconURL> initial_favicon_urls_;
  IconsMap favicon_map_;
  int id_counter_;
  base::Optional<bool> downloads_succeeded_;

  DISALLOW_COPY_AND_ASSIGN(TestWebAppIconDownloader);
};

TEST_F(WebAppIconDownloaderTest, SimpleDownload) {
  const GURL favicon_url("http://www.google.com/favicon.ico");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  std::vector<content::FaviconURL> favicon_urls;
  favicon_urls.push_back(
      content::FaviconURL(favicon_url, content::FaviconURL::IconType::kFavicon,
                          std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes(1, gfx::Size(32, 32));
  downloader.CompleteImageDownload(0, 200, favicon_urls[0].icon_url, sizes);
  EXPECT_EQ(0u, downloader.pending_requests());

  EXPECT_EQ(1u, downloader.favicon_map().size());
  EXPECT_EQ(1u, downloader.favicon_map()[favicon_url].size());
  EXPECT_TRUE(downloader.downloads_succeeded());
  histogram_tester_.ExpectUniqueSample(kHistogramForCreateName, 2, 1);
}

TEST_F(WebAppIconDownloaderTest, NoHTTPStatusCode) {
  const GURL favicon_url("data:image/png,");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  std::vector<content::FaviconURL> favicon_urls;
  favicon_urls.push_back(
      content::FaviconURL(favicon_url, content::FaviconURL::IconType::kFavicon,
                          std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes = {gfx::Size(0, 0)};
  // data: URLs have a 0 HTTP status code.
  downloader.CompleteImageDownload(0, 0, favicon_urls[0].icon_url, sizes);
  EXPECT_EQ(0u, downloader.pending_requests());

  EXPECT_EQ(1u, downloader.favicon_map().size());
  EXPECT_EQ(1u, downloader.favicon_map()[favicon_url].size());
  EXPECT_TRUE(downloader.downloads_succeeded())
      << "Should not consider data: URL or HTTP status code of 0 a failure";
  histogram_tester_.ExpectTotalCount(kHistogramForCreateName, 0);
}

TEST_F(WebAppIconDownloaderTest, DownloadWithUrlsFromWebContentsNotification) {
  const GURL favicon_url("http://www.google.com/favicon.ico");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  std::vector<content::FaviconURL> favicon_urls;
  favicon_urls.push_back(
      content::FaviconURL(favicon_url, content::FaviconURL::IconType::kFavicon,
                          std::vector<gfx::Size>()));
  EXPECT_EQ(0u, downloader.pending_requests());

  // Start downloader before favicon URLs are loaded.
  downloader.Start();
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.UpdateFaviconURLs(favicon_urls);
  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes(1, gfx::Size(32, 32));
  downloader.CompleteImageDownload(0, 200, favicon_urls[0].icon_url, sizes);
  EXPECT_EQ(0u, downloader.pending_requests());

  EXPECT_EQ(1u, downloader.favicon_map().size());
  EXPECT_EQ(1u, downloader.favicon_map()[favicon_url].size());
  EXPECT_TRUE(downloader.downloads_succeeded());
  histogram_tester_.ExpectUniqueSample(kHistogramForCreateName, 2, 1);
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
  std::vector<content::FaviconURL> favicon_urls;
  favicon_urls.push_back(content::FaviconURL(
      favicon_url_1, content::FaviconURL::IconType::kFavicon,
      std::vector<gfx::Size>()));
  // This is duplicated in the favicon urls and should only be downloaded once.
  favicon_urls.push_back(content::FaviconURL(
      empty_favicon, content::FaviconURL::IconType::kFavicon,
      std::vector<gfx::Size>()));
  // Invalid icons shouldn't get put into the download queue.
  favicon_urls.push_back(content::FaviconURL(
      GURL("http://www.google.com/invalid.ico"),
      content::FaviconURL::IconType::kInvalid, std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  downloader.Start();
  EXPECT_EQ(3u, downloader.pending_requests());

  std::vector<gfx::Size> sizes_1(1, gfx::Size(16, 16));
  downloader.CompleteImageDownload(0, 200, favicon_url_1, sizes_1);

  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(32, 32));
  sizes_2.push_back(gfx::Size(64, 64));
  downloader.CompleteImageDownload(1, 200, favicon_url_2, sizes_2);

  // Only 1 download should have been initiated for |empty_favicon| even though
  // the URL was in both the web app info and the favicon urls.
  downloader.CompleteImageDownload(2, 200, empty_favicon,
                                   std::vector<gfx::Size>());
  EXPECT_EQ(0u, downloader.pending_requests());

  EXPECT_EQ(3u, downloader.favicon_map().size());
  EXPECT_EQ(0u, downloader.favicon_map()[empty_favicon].size());
  EXPECT_EQ(1u, downloader.favicon_map()[favicon_url_1].size());
  EXPECT_EQ(2u, downloader.favicon_map()[favicon_url_2].size());
  EXPECT_TRUE(downloader.downloads_succeeded());
  histogram_tester_.ExpectUniqueSample(kHistogramForCreateName, 2, 3);
}

TEST_F(WebAppIconDownloaderTest, SkipPageFavicons) {
  const GURL favicon_url_1("http://www.google.com/favicon.ico");
  const GURL favicon_url_2("http://www.google.com/favicon2.ico");

  std::vector<GURL> extra_urls;
  extra_urls.push_back(favicon_url_1);

  TestWebAppIconDownloader downloader(web_contents(), extra_urls);

  // This favicon URL should be ignored.
  std::vector<content::FaviconURL> favicon_urls;
  favicon_urls.push_back(content::FaviconURL(
      favicon_url_2, content::FaviconURL::IconType::kFavicon,
      std::vector<gfx::Size>()));
  downloader.set_initial_favicon_urls(favicon_urls);
  downloader.SkipPageFavicons();
  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes_1(1, gfx::Size(16, 16));
  downloader.CompleteImageDownload(0, 200, favicon_url_1, sizes_1);

  // This download should not be finished and inserted into the map.
  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(32, 32));
  sizes_2.push_back(gfx::Size(64, 64));
  downloader.CompleteImageDownload(1, 200, favicon_url_2, sizes_2);
  EXPECT_EQ(0u, downloader.pending_requests());

  EXPECT_EQ(1u, downloader.favicon_map().size());
  EXPECT_EQ(1u, downloader.favicon_map()[favicon_url_1].size());
  EXPECT_EQ(0u, downloader.favicon_map()[favicon_url_2].size());
  EXPECT_TRUE(downloader.downloads_succeeded());
  histogram_tester_.ExpectUniqueSample(kHistogramForCreateName, 2, 1);
}

TEST_F(WebAppIconDownloaderTest, PageNavigates) {
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  downloader.set_initial_favicon_urls({content::FaviconURL(
      GURL("http://www.google.com/favicon.ico"),
      content::FaviconURL::IconType::kFavicon, std::vector<gfx::Size>())});
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://foo.example"), main_rfh())
      ->Commit();

  EXPECT_EQ(0u, downloader.pending_requests());
  EXPECT_TRUE(downloader.favicon_map().empty());
  EXPECT_FALSE(downloader.downloads_succeeded());
}

TEST_F(WebAppIconDownloaderTest, PageNavigatesSameDocument) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://foo.example"));

  const GURL favicon_url("http://www.google.com/favicon.ico");
  TestWebAppIconDownloader downloader(web_contents(), std::vector<GURL>());

  downloader.set_initial_favicon_urls(
      {content::FaviconURL(favicon_url, content::FaviconURL::IconType::kFavicon,
                           std::vector<gfx::Size>())});
  EXPECT_EQ(0u, downloader.pending_requests());

  downloader.Start();
  EXPECT_EQ(1u, downloader.pending_requests());

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://foo.example/#test"), main_rfh())
      ->CommitSameDocument();

  EXPECT_EQ(1u, downloader.pending_requests());

  std::vector<gfx::Size> sizes(1, gfx::Size(32, 32));
  downloader.CompleteImageDownload(0, 200, favicon_url, sizes);
  EXPECT_EQ(0u, downloader.pending_requests());

  EXPECT_EQ(1u, downloader.favicon_map().size());
  EXPECT_EQ(1u, downloader.favicon_map()[favicon_url].size());
  EXPECT_TRUE(downloader.downloads_succeeded());
  histogram_tester_.ExpectUniqueSample(kHistogramForCreateName, 2, 1);
}

}  // namespace web_app
