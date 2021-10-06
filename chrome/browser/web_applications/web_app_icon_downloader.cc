// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_downloader.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace web_app {

WebAppIconDownloader::WebAppIconDownloader(
    content::WebContents* web_contents,
    const std::vector<GURL>& extra_favicon_urls,
    Histogram histogram,
    WebAppIconDownloaderCallback callback)
    : content::WebContentsObserver(web_contents),
      need_favicon_urls_(true),
      fail_all_if_any_fail_(false),
      extra_favicon_urls_(extra_favicon_urls),
      callback_(std::move(callback)),
      histogram_(histogram) {}

WebAppIconDownloader::~WebAppIconDownloader() {}

void WebAppIconDownloader::SkipPageFavicons() {
  need_favicon_urls_ = false;
}

void WebAppIconDownloader::FailAllIfAnyFail() {
  fail_all_if_any_fail_ = true;
}

void WebAppIconDownloader::Start() {
  // Favicons are supported only in HTTP or HTTPS WebContents.
  if (!web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS())
    SkipPageFavicons();

  // If the candidates aren't loaded, icons will be fetched when
  // DidUpdateFaviconURL() is called.
  FetchIcons(extra_favicon_urls_);

  if (need_favicon_urls_) {
    // The call to `GetFaviconURLsFromWebContents()` is to allow this method to
    // be mocked by unit tests.
    const auto& favicon_urls = GetFaviconURLsFromWebContents();
    if (!favicon_urls.empty()) {
      need_favicon_urls_ = false;
      FetchIcons(favicon_urls);
    }
  }
}

int WebAppIconDownloader::DownloadImage(const GURL& url) {
  // If |is_favicon| is true, the cookies are not sent and not accepted during
  // download.
  return web_contents()->DownloadImage(
      url,
      true,         // is_favicon
      gfx::Size(),  // no preferred size
      0,            // no max size
      false,        // normal cache policy
      base::BindOnce(&WebAppIconDownloader::DidDownloadFavicon,
                     weak_ptr_factory_.GetWeakPtr()));
}

const std::vector<blink::mojom::FaviconURLPtr>&
WebAppIconDownloader::GetFaviconURLsFromWebContents() {
  return web_contents()->GetFaviconURLs();
}

void WebAppIconDownloader::FetchIcons(
    const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls) {
  std::vector<GURL> urls;
  for (const auto& favicon_url : favicon_urls) {
    if (favicon_url->icon_type != blink::mojom::FaviconIconType::kInvalid)
      urls.push_back(favicon_url->icon_url);
  }
  FetchIcons(urls);
}

void WebAppIconDownloader::FetchIcons(const std::vector<GURL>& urls) {
  // Download icons; put their download ids into |in_progress_requests_| and
  // their urls into |processed_urls_|.
  for (auto it = urls.begin(); it != urls.end(); ++it) {
    // Only start the download if the url hasn't been processed before.
    if (processed_urls_.insert(*it).second)
      in_progress_requests_.insert(DownloadImage(*it));
  }

  // If no downloads were initiated, we can proceed directly to running the
  // callback.
  if (in_progress_requests_.empty() && !need_favicon_urls_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), true, icons_map_));
  }
}

void WebAppIconDownloader::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  // Request may have been canceled by DidFinishNavigation().
  if (in_progress_requests_.erase(id) == 0)
    return;

  if (http_status_code != 0) {
    const char* histogram_name = nullptr;
    switch (histogram_) {
      case Histogram::kForCreate:
        histogram_name = "WebApp.Icon.HttpStatusCodeClassOnCreate";
        break;
      case Histogram::kForSync:
        histogram_name = "WebApp.Icon.HttpStatusCodeClassOnSync";
        break;
      case Histogram::kForUpdate:
        histogram_name = "WebApp.Icon.HttpStatusCodeClassOnUpdate";
        break;
    }
    DCHECK_LE(100, http_status_code);
    DCHECK_GT(600, http_status_code);
    base::UmaHistogramExactLinear(histogram_name, http_status_code / 100, 5);
  }

  if (fail_all_if_any_fail_ && bitmaps.empty()) {
    CancelDownloads();
    return;
  }

  icons_map_[image_url] = bitmaps;

  // Once all requests have been resolved, perform post-download tasks.
  if (in_progress_requests_.empty() && !need_favicon_urls_) {
    std::move(callback_).Run(/*success=*/true, std::move(icons_map_));
  }
}

// content::WebContentsObserver overrides:
void WebAppIconDownloader::PrimaryPageChanged(content::Page& page) {
  CancelDownloads();
}

void WebAppIconDownloader::DidUpdateFaviconURL(
    content::RenderFrameHost* rfh,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  // Only consider the first candidates we are given. This prevents pages that
  // change their favicon from spamming us.
  if (!need_favicon_urls_)
    return;

  need_favicon_urls_ = false;
  FetchIcons(candidates);
}

void WebAppIconDownloader::CancelDownloads() {
  in_progress_requests_.clear();
  icons_map_.clear();
  if (callback_)
    std::move(callback_).Run(/*success=*/false, icons_map_);
}

}  // namespace web_app
