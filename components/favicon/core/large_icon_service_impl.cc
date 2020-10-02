// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_service_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_util.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "net/base/network_change_notifier.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "url/url_canon.h"

namespace favicon {

namespace {

using favicon_base::GoogleFaviconServerRequestStatus;

const char kImageFetcherUmaClient[] = "LargeIconService";

const char kGoogleServerV2Url[] = "https://t0.gstatic.com/faviconV2";

const char kGoogleServerV2RequestFormat[] =
    "%s?client=%s&nfrp=2&%ssize=%d&min_size=%d&max_size=%d&"
    "fallback_opts=TYPE,SIZE,URL&url=%s";

const char kCheckSeenParam[] = "check_seen=true&";

const int kGoogleServerV2EnforcedMinSizeInPixel = 16;

const double kGoogleServerV2DesiredToMaxSizeFactor = 2.0;

const int kGoogleServerV2MinimumMaxSizeInPixel = 256;

GURL TrimPageUrlForGoogleServer(const GURL& page_url,
                                bool should_trim_page_url_path) {
  if (!page_url.SchemeIsHTTPOrHTTPS() || page_url.HostIsIPAddress())
    return GURL();

  url::Replacements<char> replacements;
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
    bool may_page_url_be_private,
    const GURL& server_url) {
  desired_size_in_pixel =
      std::max(desired_size_in_pixel, kGoogleServerV2EnforcedMinSizeInPixel);
  int max_size_in_pixel = static_cast<int>(
      desired_size_in_pixel * kGoogleServerV2DesiredToMaxSizeFactor);
  max_size_in_pixel =
      std::max(max_size_in_pixel, kGoogleServerV2MinimumMaxSizeInPixel);

  std::string request_url = base::StringPrintf(
      kGoogleServerV2RequestFormat, server_url.spec().c_str(),
      google_server_client_param.c_str(),
      may_page_url_be_private ? kCheckSeenParam : "", desired_size_in_pixel,
      kGoogleServerV2EnforcedMinSizeInPixel, max_size_in_pixel,
      page_url.spec().c_str());
  return GURL(request_url);
}

bool IsDbResultAdequate(const favicon_base::FaviconRawBitmapResult& db_result,
                        int min_source_size) {
  return db_result.is_valid() &&
         db_result.pixel_size.width() == db_result.pixel_size.height() &&
         db_result.pixel_size.width() >= min_source_size;
}

// Wraps the PNG data in |db_result| in a gfx::Image. If |desired_size| is not
// 0, the image gets decoded and resized to |desired_size| (in px). Must run on
// a background thread in production.
gfx::Image ResizeLargeIconOnBackgroundThread(
    const favicon_base::FaviconRawBitmapResult& db_result,
    int desired_size) {
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      db_result.bitmap_data->front(), db_result.bitmap_data->size());

  if (desired_size == 0 || db_result.pixel_size.width() == desired_size) {
    return image;
  }

  SkBitmap resized = skia::ImageOperations::Resize(
      image.AsBitmap(), skia::ImageOperations::RESIZE_LANCZOS3, desired_size,
      desired_size);
  return gfx::Image::CreateFrom1xBitmap(resized);
}

// Processes the |db_result| and writes the result into |raw_result| if
// |raw_result| is not nullptr or to |bitmap|, otherwise. If |db_result| is not
// valid or is smaller than |min_source_size|, the resulting fallback style is
// written into |fallback_icon_style|.
void ProcessIconOnBackgroundThread(
    const favicon_base::FaviconRawBitmapResult& db_result,
    int min_source_size,
    int desired_size,
    favicon_base::FaviconRawBitmapResult* raw_result,
    SkBitmap* bitmap,
    GURL* icon_url,
    favicon_base::FallbackIconStyle* fallback_icon_style) {
  if (IsDbResultAdequate(db_result, min_source_size)) {
    gfx::Image image;
    image = ResizeLargeIconOnBackgroundThread(db_result, desired_size);

    if (!image.IsEmpty()) {
      if (raw_result) {
        *raw_result = db_result;
        if (desired_size != 0)
          raw_result->pixel_size = gfx::Size(desired_size, desired_size);
        raw_result->bitmap_data = image.As1xPNGBytes();
      }
      if (bitmap) {
        *bitmap = image.AsBitmap();
      }
      if (icon_url) {
        *icon_url = db_result.icon_url;
      }
      return;
    }
  }

  if (!fallback_icon_style)
    return;

  *fallback_icon_style = favicon_base::FallbackIconStyle();
  int fallback_icon_size = 0;
  if (db_result.is_valid()) {
    favicon_base::SetDominantColorAsBackground(db_result.bitmap_data,
                                               fallback_icon_style);
    // The size must be positive, we cap to 128 to avoid the sparse histogram
    // to explode (having too many different values, server-side). Size 128
    // already indicates that there is a problem in the code, 128 px _should_ be
    // enough in all current UI surfaces.
    fallback_icon_size = db_result.pixel_size.width();
    DCHECK_GT(fallback_icon_size, 0);
    fallback_icon_size = std::min(fallback_icon_size, 128);
  }
  base::UmaHistogramSparse("Favicons.LargeIconService.FallbackSize",
                           fallback_icon_size);
}

void FinishServerRequestAsynchronously(
    favicon_base::GoogleFaviconServerCallback callback,
    favicon_base::GoogleFaviconServerRequestStatus status) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status));
}

// Processes the bitmap data returned from the FaviconService as part of a
// LargeIconService request.
class LargeIconWorker : public base::RefCountedThreadSafe<LargeIconWorker> {
 public:
  // Exactly one of the callbacks is expected to be non-null.
  LargeIconWorker(int min_source_size_in_pixel,
                  int desired_size_in_pixel,
                  favicon_base::LargeIconCallback raw_bitmap_callback,
                  favicon_base::LargeIconImageCallback image_callback,
                  base::CancelableTaskTracker* tracker);

  // Must run on the owner (UI) thread in production.
  // Intermediate callback for GetLargeIconOrFallbackStyle(). Invokes
  // ProcessIconOnBackgroundThread() so we do not perform complex image
  // operations on the UI thread.
  void OnIconLookupComplete(
      const favicon_base::FaviconRawBitmapResult& db_result);

 private:
  friend class base::RefCountedThreadSafe<LargeIconWorker>;

  ~LargeIconWorker();

  // Must run on the owner (UI) thread in production.
  // Invoked when ProcessIconOnBackgroundThread() is done.
  void OnIconProcessingComplete();

  int min_source_size_in_pixel_;
  int desired_size_in_pixel_;
  favicon_base::LargeIconCallback raw_bitmap_callback_;
  favicon_base::LargeIconImageCallback image_callback_;
  scoped_refptr<base::TaskRunner> background_task_runner_;
  base::CancelableTaskTracker* tracker_;

  favicon_base::FaviconRawBitmapResult raw_bitmap_result_;
  SkBitmap bitmap_result_;
  GURL icon_url_;
  std::unique_ptr<favicon_base::FallbackIconStyle> fallback_icon_style_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconWorker);
};

LargeIconWorker::LargeIconWorker(
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker)
    : min_source_size_in_pixel_(min_source_size_in_pixel),
      desired_size_in_pixel_(desired_size_in_pixel),
      raw_bitmap_callback_(std::move(raw_bitmap_callback)),
      image_callback_(std::move(image_callback)),
      background_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      tracker_(tracker),
      fallback_icon_style_(
          std::make_unique<favicon_base::FallbackIconStyle>()) {}

LargeIconWorker::~LargeIconWorker() {}

void LargeIconWorker::OnIconLookupComplete(
    const favicon_base::FaviconRawBitmapResult& db_result) {
  tracker_->PostTaskAndReply(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ProcessIconOnBackgroundThread, db_result,
                     min_source_size_in_pixel_, desired_size_in_pixel_,
                     raw_bitmap_callback_ ? &raw_bitmap_result_ : nullptr,
                     image_callback_ ? &bitmap_result_ : nullptr,
                     image_callback_ ? &icon_url_ : nullptr,
                     fallback_icon_style_.get()),
      base::BindOnce(&LargeIconWorker::OnIconProcessingComplete, this));
}

void LargeIconWorker::OnIconProcessingComplete() {
  // If |raw_bitmap_callback_| is provided, return the raw result.
  if (raw_bitmap_callback_) {
    if (raw_bitmap_result_.is_valid()) {
      std::move(raw_bitmap_callback_)
          .Run(favicon_base::LargeIconResult(raw_bitmap_result_));
      return;
    }
    std::move(raw_bitmap_callback_)
        .Run(favicon_base::LargeIconResult(fallback_icon_style_.release()));
    return;
  }

  if (!bitmap_result_.isNull()) {
    std::move(image_callback_)
        .Run(favicon_base::LargeIconImageResult(
            gfx::Image::CreateFrom1xBitmap(bitmap_result_), icon_url_));
    return;
  }
  std::move(image_callback_)
      .Run(favicon_base::LargeIconImageResult(fallback_icon_style_.release()));
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
  large_icon_types_.push_back({favicon_base::IconType::kWebManifestIcon});
  large_icon_types_.push_back({favicon_base::IconType::kFavicon});
  large_icon_types_.push_back({favicon_base::IconType::kTouchIcon});
  large_icon_types_.push_back({favicon_base::IconType::kTouchPrecomposedIcon});
  // TODO(jkrcal): Add non-null image_fetcher into remaining unit-tests and add
  // a DCHECK(image_fetcher_) here.
}

LargeIconServiceImpl::~LargeIconServiceImpl() {}

base::CancelableTaskTracker::TaskId
LargeIconServiceImpl::GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
    const GURL& page_url,
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    base::CancelableTaskTracker* tracker) {
  return GetLargeIconOrFallbackStyleImpl(
      page_url, min_source_size_in_pixel, desired_size_in_pixel,
      std::move(raw_bitmap_callback), favicon_base::LargeIconImageCallback(),
      tracker);
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
      favicon_base::LargeIconCallback(), std::move(image_callback), tracker);
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
      desired_size_in_pixel, desired_size_in_pixel, std::move(callback),
      favicon_base::LargeIconImageCallback(), tracker);

  return favicon_service_->GetRawFaviconForPageURL(
      page_url, {favicon_base::IconType::kFavicon}, desired_size_in_pixel,
      /*fallback_to_host=*/true,
      base::BindOnce(&LargeIconWorker::OnIconLookupComplete, worker), tracker);
}

void LargeIconServiceImpl::
    GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
        const GURL& page_url,
        bool may_page_url_be_private,
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
      desired_size_in_pixel_for_server_requests_, may_page_url_be_private,
      server_url_);
  if (!server_request_url.is_valid()) {
    FinishServerRequestAsynchronously(
        std::move(callback),
        GoogleFaviconServerRequestStatus::FAILURE_SERVER_URL_INVALID);
    return;
  }

  // Do not download if there is a previous cache miss recorded for
  // |server_request_url|.
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
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_LE(1, min_source_size_in_pixel);
  DCHECK_LE(0, desired_size_in_pixel);

  scoped_refptr<LargeIconWorker> worker = new LargeIconWorker(
      min_source_size_in_pixel, desired_size_in_pixel,
      std::move(raw_bitmap_callback), std::move(image_callback), tracker);

  int max_size_in_pixel =
      std::max(desired_size_in_pixel, min_source_size_in_pixel);
  // TODO(beaudoin): For now this is just a wrapper around
  //   GetLargestRawFaviconForPageURL. Add the logic required to select the
  //   best possible large icon. Also add logic to fetch-on-demand when the
  //   URL of a large icon is known but its bitmap is not available.
  return favicon_service_->GetLargestRawFaviconForPageURL(
      page_url, large_icon_types_, max_size_in_pixel,
      base::BindOnce(&LargeIconWorker::OnIconLookupComplete, worker), tracker);
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
