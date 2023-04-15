// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace web_app {

WebAppIconDownloader::WebAppIconDownloader(
    content::WebContents* web_contents,
    base::flat_set<GURL> extra_favicon_urls,
    WebAppIconDownloaderCallback callback,
    IconDownloaderOptions options)
    : content::WebContentsObserver(web_contents),
      extra_favicon_urls_(std::move(extra_favicon_urls)),
      options_(options),
      callback_(std::move(callback)) {
  DCHECK(web_contents);
}

WebAppIconDownloader::~WebAppIconDownloader() = default;

void WebAppIconDownloader::Start() {
  CHECK(!web_contents()->IsBeingDestroyed());
  starting_ = true;
  // Favicons are supported only in HTTP or HTTPS WebContents.
  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.is_empty() && !url.inner_url() && !url.SchemeIsHTTPOrHTTPS()) {
    options_.skip_page_favicons = true;
  }

  FetchIcons(extra_favicon_urls_);

  if (!options_.skip_page_favicons) {
    // The call to `GetFaviconURLsFromWebContents()` is to allow this method to
    // be mocked by unit tests.
    const auto& favicon_urls = GetFaviconURLsFromWebContents();
    if (!favicon_urls.empty()) {
      FetchIcons(favicon_urls);
    }
  }
  starting_ = false;
  MaybeCompleteCallback();
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
  if (!web_contents()) {
    return;
  }

  std::vector<GURL> urls;
  for (const auto& favicon_url : favicon_urls) {
    if (favicon_url->icon_type != blink::mojom::FaviconIconType::kInvalid) {
      urls.push_back(favicon_url->icon_url);
    }
  }
  FetchIcons(urls);
}

void WebAppIconDownloader::FetchIcons(const base::flat_set<GURL>& urls) {
  // Download icons; put their download ids into |in_progress_requests_| and
  // their urls into |processed_urls_|.
  for (const GURL& url : urls) {
    // Only start the download if the url hasn't been processed before.
    if (processed_urls_.insert(url).second) {
      in_progress_requests_.insert(DownloadImage(url));
    }
  }
  MaybeCompleteCallback();
}

void WebAppIconDownloader::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  // Request may have been canceled by PrimaryPageChanged().
  if (in_progress_requests_.erase(id) == 0) {
    return;
  }

  if (http_status_code != 0) {
    DCHECK_LE(100, http_status_code);
    DCHECK_GT(600, http_status_code);
    icons_http_results_[image_url] = http_status_code;
  }

  if (options_.fail_all_if_any_fail && bitmaps.empty()) {
    // Reports http status code for the failure.
    CancelDownloads(IconsDownloadedResult::kAbortedDueToFailure,
                    std::move(icons_http_results_));
    return;
  }

  icons_map_[image_url] = bitmaps;

  MaybeCompleteCallback();
}

void WebAppIconDownloader::PrimaryPageChanged(content::Page& page) {
  CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                  DownloadedIconsHttpResults{});
}

void WebAppIconDownloader::WebContentsDestroyed() {
  Observe(nullptr);
  CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                  DownloadedIconsHttpResults{});
}

void WebAppIconDownloader::MaybeCompleteCallback() {
  if (starting_ || !in_progress_requests_.empty() || callback_.is_null()) {
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), IconsDownloadedResult::kCompleted,
                     std::move(icons_map_), std::move(icons_http_results_)));
}

void WebAppIconDownloader::CancelDownloads(
    IconsDownloadedResult result,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK_NE(result, IconsDownloadedResult::kCompleted);

  in_progress_requests_.clear();
  icons_map_.clear();
  icons_http_results_.clear();

  if (callback_) {
    std::move(callback_).Run(result, IconsMap{}, std::move(icons_http_results));
  }
}

}  // namespace web_app
