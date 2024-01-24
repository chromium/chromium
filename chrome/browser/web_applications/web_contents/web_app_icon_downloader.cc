// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace web_app {
namespace {
BASE_FEATURE(kIconDownloaderTimeout,
             "WebAppIconDownloaderTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);
base::FeatureParam<base::TimeDelta> kTimeoutTime(
    &kIconDownloaderTimeout,
    "timeout_time",
    WebAppIconDownloader::kDefaultSecondsToWaitForIconDownloading);

// TODO(b/302531937): Make this a utility that can be used through out the
// web_applications/ system.
bool WebContentsShuttingDown(content::WebContents* web_contents) {
  return !web_contents || web_contents->IsBeingDestroyed() ||
         web_contents->GetBrowserContext()->ShutdownStarted();
}

}  // namespace

WebAppIconDownloader::WebAppIconDownloader() = default;
WebAppIconDownloader::~WebAppIconDownloader() = default;

void WebAppIconDownloader::Start(
    content::WebContents* web_contents,
    const IconUrlSizeSet& extra_icon_urls_with_sizes,
    WebAppIconDownloaderCallback callback,
    IconDownloaderOptions options) {
  // Cannot call start more than once.
  CHECK_EQ(pending_requests(), 0ul);
  CHECK(web_contents);
  CHECK(!web_contents->IsBeingDestroyed());
  Observe(web_contents);
  callback_ = std::move(callback);
  options_ = options;

  if (WebContentsShuttingDown(web_contents)) {
    // Reports http status code for the failure.
    CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                    DownloadedIconsHttpResults{},
                    WebAppIconDownloaderResult::kPrimaryPageChanged);
    return;
  }

  if (base::FeatureList::IsEnabled(kIconDownloaderTimeout)) {
    timer_.Start(FROM_HERE, kTimeoutTime.Get(),
                 base::BindOnce(&WebAppIconDownloader::OnTimeout,
                                // OneShotTimer is owned by this class and
                                // it guarantees that it will never run after
                                // it's destroyed.
                                base::Unretained(this)));
  }

  // Favicons are supported only in HTTP or HTTPS WebContents.
  const GURL& url = web_contents->GetLastCommittedURL();
  if (!url.is_empty() && !url.inner_url() && !url.SchemeIsHTTPOrHTTPS()) {
    options_.skip_page_favicons = true;
  }

  // The call to `GetFaviconURLsFromWebContents()` is to allow this method to
  // be mocked by unit tests.
  const auto& favicon_urls = GetFaviconURLsFromWebContents();

  if (!options_.skip_page_favicons && !favicon_urls.empty()) {
    std::vector<IconUrlWithSize> combined_icon_infos(
        extra_icon_urls_with_sizes.begin(), extra_icon_urls_with_sizes.end());
    combined_icon_infos.reserve(combined_icon_infos.size() +
                                favicon_urls.size());
    for (const auto& favicon_url : favicon_urls) {
      if (favicon_url->icon_type != blink::mojom::FaviconIconType::kInvalid) {
        combined_icon_infos.emplace_back(
            IconUrlWithSize::CreateForUnspecifiedSize(favicon_url->icon_url));
      }
    }
    FetchIcons(IconUrlSizeSet(combined_icon_infos));
  } else {
    FetchIcons(extra_icon_urls_with_sizes);
  }
}

bool WebAppIconDownloader::IsRunning() {
  return !callback_.is_null();
}

int WebAppIconDownloader::DownloadImage(const GURL& url,
                                        const gfx::Size& preferred_size) {
  if (web_contents()->GetBrowserContext()->ShutdownStarted()) {
    // Reports http status code for the failure.
    CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                    DownloadedIconsHttpResults{},
                    WebAppIconDownloaderResult::kPrimaryPageChanged);
    return 0;
  } else {
    // If |is_favicon| is true, the cookies are not sent and not accepted during
    // download.
    return web_contents()->DownloadImage(
        url,
        true,  // is_favicon
        preferred_size,
        0,      // no max size
        false,  // normal cache policy
        base::BindOnce(&WebAppIconDownloader::DidDownloadFavicon,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

const std::vector<blink::mojom::FaviconURLPtr>&
WebAppIconDownloader::GetFaviconURLsFromWebContents() {
  return web_contents()->GetFaviconURLs();
}

void WebAppIconDownloader::FetchIcons(
    const IconUrlSizeSet& urls_to_download_with_size) {
  CHECK_EQ(pending_requests(), 0ul);
  CHECK(!populating_pending_requests_);

  // This is required because `DidDownloadFavicon` is triggered synchronously in
  // some tests.
  populating_pending_requests_ = true;
  // Download icons; put their download ids into |in_progress_requests_| and
  // their urls into |processed_urls_|.
  for (const auto& url_data : urls_to_download_with_size) {
    // Only start the download if the url hasn't been processed before.
    if (processed_urls_.insert(url_data).second) {
      const GURL url = url_data.url;
      in_progress_requests_.insert(
          std::make_pair(DownloadImage(url, url_data.size), url_data));
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
  if (!IsRunning()) {
    return;
  }

  if (web_contents()->GetBrowserContext()->ShutdownStarted()) {
    CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                    DownloadedIconsHttpResults{},
                    WebAppIconDownloaderResult::kPrimaryPageChanged);
    return;
  }

  const IconUrlWithSize icon_urls_with_sizes = in_progress_requests_.at(id);
  size_t num_deleted = in_progress_requests_.erase(id);
  CHECK_EQ(num_deleted, 1ul);

  base::UmaHistogramSparse("WebApp.IconDownloader.HttpResult",
                           http_status_code);

  if (http_status_code != 0) {
    DCHECK_LE(100, http_status_code);
    DCHECK_GT(600, http_status_code);
    icons_http_results_[icon_urls_with_sizes] = http_status_code;
  }

  if (options_.fail_all_if_any_fail && bitmaps.empty()) {
    // Reports http status code for the failure.
    CancelDownloads(IconsDownloadedResult::kAbortedDueToFailure,
                    std::move(icons_http_results_),
                    WebAppIconDownloaderResult::kFailure);
    return;
  }

  // TODO(b/319669415 : Support adding onto existing bitmaps array instead of
  // overwriting. Helpful for downloading multiple bitmaps from the same URL.
  icons_map_[image_url] = bitmaps;

  MaybeCompleteCallback();
}

void WebAppIconDownloader::PrimaryPageChanged(content::Page& page) {
  CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                  DownloadedIconsHttpResults{},
                  WebAppIconDownloaderResult::kPrimaryPageChanged);
}

void WebAppIconDownloader::WebContentsDestroyed() {
  Observe(nullptr);
  CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                  DownloadedIconsHttpResults{},
                  WebAppIconDownloaderResult::kPrimaryPageChanged);
}

void WebAppIconDownloader::MaybeCompleteCallback() {
  if (!IsRunning()) {
    return;
  }
  if (populating_pending_requests_ || !in_progress_requests_.empty()) {
    return;
  }
  timer_.Stop();
  base::UmaHistogramEnumeration("WebApp.IconDownloader.Result",
                                WebAppIconDownloaderResult::kSuccess);
  CHECK(callback_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), IconsDownloadedResult::kCompleted,
                     std::move(icons_map_), std::move(icons_http_results_)));
}

void WebAppIconDownloader::CancelDownloads(
    IconsDownloadedResult result,
    DownloadedIconsHttpResults icons_http_results,
    WebAppIconDownloaderResult metrics_result) {
  if (!IsRunning()) {
    return;
  }
  DCHECK_NE(result, IconsDownloadedResult::kCompleted);

  weak_ptr_factory_.InvalidateWeakPtrs();
  in_progress_requests_.clear();
  icons_map_.clear();
  icons_http_results_.clear();
  timer_.Stop();

  base::UmaHistogramEnumeration("WebApp.IconDownloader.Result", metrics_result);
  CHECK(callback_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), result, IconsMap{},
                                std::move(icons_http_results_)));
}

void WebAppIconDownloader::OnTimeout() {
  if (!IsRunning()) {
    return;
  }

  if (web_contents()->GetBrowserContext()->ShutdownStarted()) {
    // Reports http status code for the failure.
    CancelDownloads(IconsDownloadedResult::kPrimaryPageChanged,
                    DownloadedIconsHttpResults{},
                    WebAppIconDownloaderResult::kPrimaryPageChanged);
    return;
  }

  if (options_.fail_all_if_any_fail || icons_map_.empty()) {
    CancelDownloads(IconsDownloadedResult::kAbortedDueToFailure,
                    DownloadedIconsHttpResults(),
                    WebAppIconDownloaderResult::kTimeoutFailure);
    return;
  }

  // Populate results for the the hanging requests.
  for (const auto& [_, icon_url_with_sizes] : in_progress_requests_) {
    icons_http_results_[icon_url_with_sizes] =
        net::HttpStatusCode::HTTP_REQUEST_TIMEOUT;
  }
  base::UmaHistogramEnumeration(
      "WebApp.IconDownloader.Result",
      WebAppIconDownloaderResult::kTimeoutWithPartialDownloadSuccess);
  weak_ptr_factory_.InvalidateWeakPtrs();
  in_progress_requests_.clear();
  CHECK(callback_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), IconsDownloadedResult::kCompleted,
                     std::move(icons_map_), std::move(icons_http_results_)));
}

}  // namespace web_app
