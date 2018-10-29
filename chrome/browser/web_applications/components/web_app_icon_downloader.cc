// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_icon_downloader.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/histogram_functions.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/favicon_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

WebAppIconDownloader::WebAppIconDownloader(
    content::WebContents* web_contents,
    const std::vector<GURL>& extra_favicon_urls,
    const char* https_status_code_class_histogram_name,
    WebAppIconDownloaderCallback callback)
    : content::WebContentsObserver(web_contents),
      need_favicon_urls_(true),
      extra_favicon_urls_(extra_favicon_urls),
      callback_(std::move(callback)),
      https_status_code_class_histogram_name_(
          https_status_code_class_histogram_name),
      weak_ptr_factory_(this) {}

WebAppIconDownloader::~WebAppIconDownloader() {}

void WebAppIconDownloader::SkipPageFavicons() {
  need_favicon_urls_ = false;
}

void WebAppIconDownloader::Start() {
  // If the candidates aren't loaded, icons will be fetched when
  // DidUpdateFaviconURL() is called.
  FetchIcons(extra_favicon_urls_);

  if (need_favicon_urls_) {
    std::vector<content::FaviconURL> favicon_tab_helper_urls =
        GetFaviconURLsFromWebContents();
    if (!favicon_tab_helper_urls.empty()) {
      need_favicon_urls_ = false;
      FetchIcons(favicon_tab_helper_urls);
    }
  }
}

int WebAppIconDownloader::DownloadImage(const GURL& url) {
  return web_contents()->DownloadImage(
      url,
      true,   // is_favicon
      0,      // no max size
      false,  // normal cache policy
      base::BindOnce(&WebAppIconDownloader::DidDownloadFavicon,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::vector<content::FaviconURL>
WebAppIconDownloader::GetFaviconURLsFromWebContents() {
  favicon::ContentFaviconDriver* content_favicon_driver =
      web_contents()
          ? favicon::ContentFaviconDriver::FromWebContents(web_contents())
          : nullptr;
  // If favicon_urls() is empty, we are guaranteed that DidUpdateFaviconURLs has
  // not yet been called for the current page's navigation.
  return content_favicon_driver ? content_favicon_driver->favicon_urls()
                                : std::vector<content::FaviconURL>();
}

void WebAppIconDownloader::FetchIcons(
    const std::vector<content::FaviconURL>& favicon_urls) {
  std::vector<GURL> urls;
  for (auto it = favicon_urls.begin(); it != favicon_urls.end(); ++it) {
    if (it->icon_type != content::FaviconURL::IconType::kInvalid)
      urls.push_back(it->icon_url);
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
    base::MessageLoopCurrent::Get()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), true, favicon_map_));
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

  if (!https_status_code_class_histogram_name_.empty()) {
    DCHECK_LE(100, http_status_code);
    DCHECK_GT(600, http_status_code);
    base::UmaHistogramExactLinear(https_status_code_class_histogram_name_,
                                  http_status_code / 100, 5);
  }

  favicon_map_[image_url] = bitmaps;

  // Once all requests have been resolved, perform post-download tasks.
  if (in_progress_requests_.empty() && !need_favicon_urls_)
    std::move(callback_).Run(true, favicon_map_);
}

// content::WebContentsObserver overrides:
void WebAppIconDownloader::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsSameDocument())
    return;

  // Clear all pending requests.
  in_progress_requests_.clear();
  favicon_map_.clear();
  std::move(callback_).Run(false, favicon_map_);
}

void WebAppIconDownloader::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  // Only consider the first candidates we are given. This prevents pages that
  // change their favicon from spamming us.
  if (!need_favicon_urls_)
    return;

  need_favicon_urls_ = false;
  FetchIcons(candidates);
}
