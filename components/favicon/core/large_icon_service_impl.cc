// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service_impl.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_worker.h"
#include "components/favicon_base/favicon_util.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/base/network_change_notifier.h"
#include "url/url_canon.h"

namespace favicon {

namespace {

using favicon_base::GoogleFaviconServerRequestStatus;
using StandardIconSize = favicon::LargeIconService::StandardIconSize;
using NoBigEnoughIconBehavior =
    favicon::LargeIconService::NoBigEnoughIconBehavior;

const char kImageFetcherUmaClient[] = "LargeIconService";

const char kGoogleServerV2Url[] = "https://t0.gstatic.com/faviconV2";

// `check_seen` is a legacy parameter which prevents the Google-favicon-server
// from crawling a URL as a result of a Google-favicon-server request in order
// to prevent Google from trying to crawl enterprise/private URLs. Currently the
// Google-favicon-server ignores the `check_seen` parameter and never triggers
// Google crawling a URL. check_seen is set explicitly set explicitly in the
// request URL to make sure `LargeIconService` behavior is not affected if this
// changes at any point in time.
const char kGoogleServerV2RequestFormat[] =
    "%s?client=%s&nfrp=2&check_seen=true&size=%d&min_size=%d&max_size=%d&"
    "fallback_opts=TYPE,SIZE,URL&url=%s";

const int kGoogleServerV2EnforcedMinSizeInPixel = 16;

const double kGoogleServerV2DesiredToMaxSizeFactor = 2.0;

const int kGoogleServerV2MinimumMaxSizeInPixel = 256;

GURL TrimPageUrlForGoogleServer(const GURL& page_url,
                                bool should_trim_page_url_path) {
  if (!page_url.SchemeIsHTTPOrHTTPS() || page_url.HostIsIPAddress())
    return GURL();

  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  if (should_trim_page_url_path)
    replacements.ClearPath();
  return page_url.ReplaceComponents(replacements);
}

GURL GetRequestUrlForGoogleServerV2(
    const GURL& page_url,
    const std::string& google_server_client_param,
    int desired_size_in_pixel,
    const GURL& server_url) {
  // Server expects a size value from the server-side enum
  // favicon_service.FaviconSize
  DCHECK(desired_size_in_pixel == 16 || desired_size_in_pixel == 24 ||
         desired_size_in_pixel == 32 || desired_size_in_pixel == 48 ||
         desired_size_in_pixel == 50 || desired_size_in_pixel == 64 ||
         desired_size_in_pixel == 96 || desired_size_in_pixel == 128 ||
         desired_size_in_pixel == 180 || desired_size_in_pixel == 256)
      << "Icon size not supported by the favicon service: "
      << desired_size_in_pixel;
  desired_size_in_pixel =
      std::max(desired_size_in_pixel, kGoogleServerV2EnforcedMinSizeInPixel);
  int max_size_in_pixel = static_cast<int>(
      desired_size_in_pixel * kGoogleServerV2DesiredToMaxSizeFactor);
  max_size_in_pixel =
      std::max(max_size_in_pixel, kGoogleServerV2MinimumMaxSizeInPixel);

  std::string request_url = base::StringPrintf(
      kGoogleServerV2RequestFormat, server_url.spec().c_str(),
      google_server_client_param.c_str(), desired_size_in_pixel,
      kGoogleServerV2EnforcedMinSizeInPixel, max_size_in_pixel,
      page_url.spec().c_str());
  return GURL(request_url);
}

void FinishServerRequestAsynchronously(
    favicon_base::GoogleFaviconServerCallback callback,
    favicon_base::GoogleFaviconServerRequestStatus status) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status));
}

void ReportDownloadedSize(int size) {
  UMA_HISTOGRAM_COUNTS_1000("Favicons.LargeIconService.DownloadedSize", size);
}

void OnSetOnDemandFaviconComplete(
    favicon_base::GoogleFaviconServerCallback callback,
    bool success) {
  std::move(callback).Run(
      success ? GoogleFaviconServerRequestStatus::SUCCESS
              : GoogleFaviconServerRequestStatus::FAILURE_ON_WRITE);
}

void OnFetchIconFromGoogleServerComplete(
    FaviconService* favicon_service,
    const GURL& page_url,
    const GURL& server_request_url,
    favicon_base::IconType icon_type,
    favicon_base::GoogleFaviconServerCallback callback,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (image.IsEmpty()) {
    DLOG(WARNING) << "large icon server fetch empty "
                  << server_request_url.spec();
    favicon_service->UnableToDownloadFavicon(server_request_url);
    std::move(callback).Run(
        metadata.http_response_code ==
                image_fetcher::RequestMetadata::RESPONSE_CODE_INVALID
            ? GoogleFaviconServerRequestStatus::FAILURE_CONNECTION_ERROR
            : GoogleFaviconServerRequestStatus::FAILURE_HTTP_ERROR);
    ReportDownloadedSize(0);
    return;
  }

  ReportDownloadedSize(image.Width());

  // If given, use the original favicon URL from Content-Location http header.
  // Otherwise, use the request URL as fallback.
  std::string original_icon_url = metadata.content_location_header;
  if (original_icon_url.empty()) {
    original_icon_url = server_request_url.spec();
  }

  // Write fetched icons to FaviconService's cache, but only if no icon was
  // available (clients are encouraged to do this in advance, but meanwhile
  // something else could've been written). By marking the icons initially
  // expired (out-of-date), they will be refetched when we visit the original
  // page any time in the future.
  favicon_service->SetOnDemandFavicons(
      page_url, GURL(original_icon_url), icon_type, image,
      base::BindOnce(&OnSetOnDemandFaviconComplete, std::move(callback)));
}

float GetMaxDeviceScale() {
  std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  DCHECK(!favicon_scales.empty());
  return favicon_scales.back();
}

int IconSizeToInt(StandardIconSize icon_size) {
  switch (icon_size) {
    case StandardIconSize::k16x16:
      return 16;
    case StandardIconSize::k32x32:
      return 32;
  }
}

}  // namespace

LargeIconServiceImpl::LargeIconServiceImpl(
    FaviconService* favicon_service,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
    int desired_size_in_dip_for_server_requests,
    favicon_base::IconType icon_type_for_server_requests,
    const std::string& google_server_client_param)
    : favicon_service_(favicon_service),
      image_fetcher_(std::move(image_fetcher)),
      desired_size_in_pixel_for_server_requests_(std::ceil(
          desired_size_in_dip_for_server_requests * GetMaxDeviceScale())),
      icon_type_for_server_requests_(icon_type_for_server_requests),
      google_server_client_param_(google_server_client_param),
      server_url_(kGoogleServerV2Url) {
  // TODO(jkrcal): Add non-null image_fetcher into remaining unit-tests and add
  // a DCHECK(image_fetcher_) here.
}

LargeIconServiceImpl::~LargeIconServiceImpl() = default;

base::CancelableTaskTracker::TaskId
LargeIconServiceImpl::GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
    const GURL& page_url,
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    base::CancelableTaskTracker* tracker) {
  return GetLargeIconRawBitmapForPageUrl(
      page_url, min_source_size_in_pixel, desired_size_in_pixel,
      NoBigEnoughIconBehavior::kReturnFallbackColor,
      std::move(raw_bitmap_callback), tracker);
}

base::CancelableTaskTracker::TaskId
LargeIconServiceImpl::GetLargeIconImageOrFallbackStyleForPageUrl(
    const GURL& page_url,
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker) {
  return GetLargeIconOrFallbackStyleImpl(
      page_url, min_source_size_in_pixel, desired_size_in_pixel,
      NoBigEnoughIconBehavior::kReturnFallbackColor,
      favicon_base::LargeIconCallback(), std::move(image_callback), tracker);
}

base::CancelableTaskTracker::TaskId
LargeIconServiceImpl::GetLargeIconRawBitmapForPageUrl(
    const GURL& page_url,
    int min_source_size_in_pixel,
    std::optional<int> size_in_pixel_to_resize_to,
    NoBigEnoughIconBehavior no_big_enough_icon_behavior,
    favicon_base::LargeIconCallback callback,
    base::CancelableTaskTracker* tracker) {
  return GetLargeIconOrFallbackStyleImpl(
      page_url, min_source_size_in_pixel, size_in_pixel_to_resize_to,
      no_big_enough_icon_behavior, std::move(callback),
      favicon_base::LargeIconImageCallback(), tracker);
}

base::CancelableTaskTracker::TaskId
LargeIconServiceImpl::GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
    const GURL& icon_url,
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_LE(1, min_source_size_in_pixel);
  DCHECK_LE(0, desired_size_in_pixel);

  scoped_refptr<LargeIconWorker> worker = base::MakeRefCounted<LargeIconWorker>(
      min_source_size_in_pixel, desired_size_in_pixel,
      NoBigEnoughIconBehavior::kReturnFallbackColor,
      std::move(raw_bitmap_callback), favicon_base::LargeIconImageCallback(),
      tracker);

  int max_size_in_pixel =
      std::max(desired_size_in_pixel, min_source_size_in_pixel);
  return favicon_service_->GetRawFavicon(
      icon_url, favicon_base::IconType::kFavicon, max_size_in_pixel,
      base::BindOnce(&LargeIconWorker::OnIconLookupComplete, worker), tracker);
}

base::CancelableTaskTracker::TaskId
LargeIconServiceImpl::GetIconRawBitmapOrFallbackStyleForPageUrl(
    const GURL& page_url,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_LE(0, desired_size_in_pixel);

  scoped_refptr<LargeIconWorker> worker = base::MakeRefCounted<LargeIconWorker>(
      desired_size_in_pixel, desired_size_in_pixel,
      NoBigEnoughIconBehavior::kReturnFallbackColor, std::move(callback),
      favicon_base::LargeIconImageCallback(), tracker);

  return favicon_service_->GetRawFaviconForPageURL(
      page_url, {favicon_base::IconType::kFavicon}, desired_size_in_pixel,
      /*fallback_to_host=*/true,
      base::BindOnce(&LargeIconWorker::OnIconLookupComplete, worker), tracker);
}

void LargeIconServiceImpl::
    GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
        const GURL& page_url,
        bool should_trim_page_url_path,
        const net::NetworkTrafficAnnotationTag& traffic_annotation,
        favicon_base::GoogleFaviconServerCallback callback) {
  if (net::NetworkChangeNotifier::IsOffline()) {
    // By exiting early when offline, we avoid caching the failure and thus
    // allow icon fetches later when coming back online.
    FinishServerRequestAsynchronously(
        std::move(callback),
        GoogleFaviconServerRequestStatus::FAILURE_CONNECTION_ERROR);
    return;
  }

  if (!page_url.is_valid()) {
    FinishServerRequestAsynchronously(
        std::move(callback),
        GoogleFaviconServerRequestStatus::FAILURE_TARGET_URL_INVALID);
    return;
  }

  const GURL trimmed_page_url =
      TrimPageUrlForGoogleServer(page_url, should_trim_page_url_path);
  if (!trimmed_page_url.is_valid()) {
    FinishServerRequestAsynchronously(
        std::move(callback),
        GoogleFaviconServerRequestStatus::FAILURE_TARGET_URL_SKIPPED);
    return;
  }

  const GURL server_request_url = GetRequestUrlForGoogleServerV2(
      trimmed_page_url, google_server_client_param_,
      desired_size_in_pixel_for_server_requests_, server_url_);
  if (!server_request_url.is_valid()) {
    FinishServerRequestAsynchronously(
        std::move(callback),
        GoogleFaviconServerRequestStatus::FAILURE_SERVER_URL_INVALID);
    return;
  }

  // Do not download if there is a previous cache miss recorded for
  // `server_request_url`.
  if (favicon_service_->WasUnableToDownloadFavicon(server_request_url)) {
    FinishServerRequestAsynchronously(
        std::move(callback),
        GoogleFaviconServerRequestStatus::FAILURE_HTTP_ERROR_CACHED);
    return;
  }

  favicon_service_->CanSetOnDemandFavicons(
      page_url, icon_type_for_server_requests_,
      base::BindOnce(&LargeIconServiceImpl::OnCanSetOnDemandFaviconComplete,
                     weak_ptr_factory_.GetWeakPtr(), server_request_url,
                     page_url, traffic_annotation, std::move(callback)));
}

void LargeIconServiceImpl::GetLargeIconFromCacheFallbackToGoogleServer(
    const GURL& page_url,
    StandardIconSize min_source_size,
    std::optional<StandardIconSize> size_to_resize_to,
    NoBigEnoughIconBehavior no_big_enough_icon_behavior,
    bool should_trim_page_url_path,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    favicon_base::LargeIconCallback callback,
    base::CancelableTaskTracker* tracker) {
  const std::optional<int> size_to_resize_to_int =
      size_to_resize_to
          ? std::optional(IconSizeToInt(size_to_resize_to.value()))
          : std::nullopt;
  // If the `no_big_enough_icon_behavior` is equal to `kReturnEmpty`, it gets
  // overridden to `kReturnFallbackColor`. This is done to optimize the number
  // of the database lookups in the case the database contains an icon for
  // `page_url`, but it's not big enough. In this case,
  // `GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache()` favicon
  // service will fail because only 1 icon can be stored per domain in the
  // database and
  // GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache() is not
  // allowed to overwrite icons stored in the database
  const NoBigEnoughIconBehavior requested_no_big_enough_icon_behavior =
      no_big_enough_icon_behavior == NoBigEnoughIconBehavior::kReturnEmpty
          ? NoBigEnoughIconBehavior::kReturnFallbackColor
          : no_big_enough_icon_behavior;
  GetLargeIconRawBitmapForPageUrl(
      page_url, IconSizeToInt(min_source_size), size_to_resize_to_int,
      requested_no_big_enough_icon_behavior,
      base::BindOnce(&LargeIconServiceImpl::OnIconFetchedFromCache,
                     weak_ptr_factory_.GetWeakPtr(), page_url,
                     IconSizeToInt(min_source_size), size_to_resize_to_int,
                     no_big_enough_icon_behavior, should_trim_page_url_path,
                     traffic_annotation, std::move(callback), tracker),
      tracker);
}

void LargeIconServiceImpl::OnIconFetchedFromCache(
    const GURL& page_url,
    int min_source_size_in_pixel,
    std::optional<int> size_in_pixel_to_resize_to,
    NoBigEnoughIconBehavior no_big_enough_icon_behavior,
    bool should_trim_page_url_path,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    favicon_base::LargeIconCallback callback,
    base::CancelableTaskTracker* tracker,
    const favicon_base::LargeIconResult& icon_result) {
  if (icon_result.bitmap.is_valid() ||
      (no_big_enough_icon_behavior ==
           NoBigEnoughIconBehavior::kReturnFallbackColor &&
       icon_result.fallback_icon_style)) {
    std::move(callback).Run(icon_result);
    return;
  }
  if (no_big_enough_icon_behavior == NoBigEnoughIconBehavior::kReturnEmpty &&
      icon_result.fallback_icon_style) {
    // An icon (smaller than `min_source_size_in_pixel`) is already stored in
    // the database,
    // GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache() will
    // fail.
    std::move(callback).Run(
        favicon_base::LargeIconResult(favicon_base::FaviconRawBitmapResult()));
    return;
  }

  // `was_task_canceled_callback` tracks whether `tracker` has been destroyed.
  base::CancelableTaskTracker::IsCanceledCallback was_task_canceled_callback;
  tracker->NewTrackedTaskId(&was_task_canceled_callback);

  GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
      page_url, should_trim_page_url_path, traffic_annotation,
      base::BindOnce(&LargeIconServiceImpl::OnIconFetchedFromServer,
                     base::Unretained(this), page_url, min_source_size_in_pixel,
                     size_in_pixel_to_resize_to, std::move(callback),
                     std::move(was_task_canceled_callback),
                     base::UnsafeDangling(tracker)));
}

void LargeIconServiceImpl::OnIconFetchedFromServer(
    const GURL& page_url,
    int min_source_size_in_pixel,
    std::optional<int> size_in_pixel_to_resize_to,
    favicon_base::LargeIconCallback callback,
    base::CancelableTaskTracker::IsCanceledCallback was_task_canceled_callback,
    MayBeDangling<base::CancelableTaskTracker> tracker,
    favicon_base::GoogleFaviconServerRequestStatus status) {
  // Check whether `tracker` has been destroyed.
  if (was_task_canceled_callback.Run()) {
    return;
  }

  if (status == favicon_base::GoogleFaviconServerRequestStatus::SUCCESS) {
    GetLargeIconOrFallbackStyleImpl(
        page_url, min_source_size_in_pixel, size_in_pixel_to_resize_to,
        NoBigEnoughIconBehavior::kReturnEmpty, std::move(callback),
        favicon_base::LargeIconImageCallback(), tracker);
    return;
  }
  std::move(callback).Run(
      favicon_base::LargeIconResult(favicon_base::FaviconRawBitmapResult()));
}

void LargeIconServiceImpl::TouchIconFromGoogleServer(const GURL& icon_url) {
  favicon_service_->TouchOnDemandFavicon(icon_url);
}

void LargeIconServiceImpl::SetServerUrlForTesting(
    const GURL& server_url_for_testing) {
  server_url_ = server_url_for_testing;
}

base::CancelableTaskTracker::TaskId
LargeIconServiceImpl::GetLargeIconOrFallbackStyleImpl(
    const GURL& page_url,
    int min_source_size_in_pixel,
    std::optional<int> size_in_pixel_to_resize_to,
    NoBigEnoughIconBehavior no_big_enough_icon_behavior,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker) {
  return LargeIconWorker::GetLargeIconRawBitmap(
      favicon_service_, page_url, min_source_size_in_pixel,
      size_in_pixel_to_resize_to.value_or(0), no_big_enough_icon_behavior,
      std::move(raw_bitmap_callback), std::move(image_callback), tracker);
}

void LargeIconServiceImpl::OnCanSetOnDemandFaviconComplete(
    const GURL& server_request_url,
    const GURL& page_url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    favicon_base::GoogleFaviconServerCallback callback,
    bool can_set_on_demand_favicon) {
  if (!can_set_on_demand_favicon) {
    std::move(callback).Run(
        GoogleFaviconServerRequestStatus::FAILURE_ICON_EXISTS_IN_DB);
    return;
  }

  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           kImageFetcherUmaClient);
  image_fetcher_->FetchImage(
      server_request_url,
      base::BindOnce(&OnFetchIconFromGoogleServerComplete, favicon_service_,
                     page_url, server_request_url,
                     icon_type_for_server_requests_, std::move(callback)),
      std::move(params));
}

}  // namespace favicon
