// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace web_app {

WebAppIconDownloader::WebAppIconDownloader() = default;
WebAppIconDownloader::~WebAppIconDownloader() = default;

void WebAppIconDownloader::Start(content::WebContents* web_contents,
                                 const base::flat_set<GURL>& extra_icon_urls,
                                 WebAppIconDownloaderCallback callback,
                                 IconDownloaderOptions options) {
  // Cannot call start more than once.
  CHECK_EQ(pending_requests(), 0ul);
  CHECK(web_contents);
  CHECK(!web_contents->IsBeingDestroyed());
  Observe(web_contents);
  callback_ = std::move(callback);
  options_ = options;

  // Favicons are supported only in HTTP or HTTPS WebContents.
  const GURL& url = web_contents->GetLastCommittedURL();
  if (!url.is_empty() && !url.inner_url() && !url.SchemeIsHTTPOrHTTPS()) {
    options_.skip_page_favicons = true;
  }

  // The call to `GetFaviconURLsFromWebContents()` is to allow this method to
  // be mocked by unit tests.
  const auto& favicon_urls = GetFaviconURLsFromWebContents();

  if (!options_.skip_page_favicons && !favicon_urls.empty()) {
    std::vector<GURL> combined_icon_urls(extra_icon_urls.begin(),
                                         extra_icon_urls.end());
    combined_icon_urls.reserve(combined_icon_urls.size() + favicon_urls.size());
    for (const auto& favicon_url : favicon_urls) {
      if (favicon_url->icon_type != blink::mojom::FaviconIconType::kInvalid) {
        combined_icon_urls.push_back(favicon_url->icon_url);
      }
    }
    FetchIcons(base::flat_set<GURL>(combined_icon_urls));
  } else {
    FetchIcons(extra_icon_urls);
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

void WebAppIconDownloader::FetchIcons(const base::flat_set<GURL>& urls) {
  CHECK_EQ(pending_requests(), 0ul);
  CHECK(!populating_pending_requests_);

  // This is required because `DidDownloadFavicon` is triggered synchronously in
  // some tests.
  populating_pending_requests_ = true;
  // Download icons; put their download ids into |in_progress_requests_| and
  // their urls into |processed_urls_|.
  for (const GURL& url : urls) {
    // Only start the download if the url hasn't been processed before.
    if (processed_urls_.insert(url).second) {
      in_progress_requests_.insert(DownloadImage(url));
    }
  }
  populating_pending_requests_ = false;

  MaybeCompleteCallback();
}

void WebAppIconDownloader::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  size_t num_deleted = in_progress_requests_.erase(id);
  CHECK_EQ(num_deleted, 1ul);

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
  if (callback_) {
    CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                    DownloadedIconsHttpResults{});
  }
}

void WebAppIconDownloader::WebContentsDestroyed() {
  Observe(nullptr);
  if (callback_) {
    CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                    DownloadedIconsHttpResults{});
  }
}

void WebAppIconDownloader::MaybeCompleteCallback() {
  if (populating_pending_requests_ || !in_progress_requests_.empty() ||
      callback_.is_null()) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), IconsDownloadedResult::kCompleted,
                     std::move(icons_map_), std::move(icons_http_results_)));
}

void WebAppIconDownloader::CancelDownloads(
    IconsDownloadedResult result,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK_NE(result, IconsDownloadedResult::kCompleted);

  weak_ptr_factory_.InvalidateWeakPtrs();
  in_progress_requests_.clear();
  icons_map_.clear();
  icons_http_results_.clear();

  if (callback_) {
    std::move(callback_).Run(result, IconsMap{}, std::move(icons_http_results));
  }
}

}  // namespace web_app
