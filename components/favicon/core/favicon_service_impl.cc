// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_service_impl.h"

#include <stddef.h>
#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/history/core/browser/history_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

namespace favicon {

// static
void FaviconServiceImpl::RunFaviconImageCallbackWithBitmapResults(
    favicon_base::FaviconImageCallback callback,
    int desired_size_in_dip,
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results) {
  TRACE_EVENT0("browser",
               "FaviconServiceImpl::RunFaviconImageCallbackWithBitmapResults");
  favicon_base::FaviconImageResult image_result;
  image_result.image = favicon_base::SelectFaviconFramesFromPNGs(
      favicon_bitmap_results, favicon_base::GetFaviconScales(),
      desired_size_in_dip);

  image_result.icon_url = image_result.image.IsEmpty()
                              ? GURL()
                              : favicon_bitmap_results[0].icon_url;
  std::move(callback).Run(image_result);
}

FaviconServiceImpl::FaviconServiceImpl(
    std::unique_ptr<FaviconClient> favicon_client,
    history::HistoryService* history_service)
    : favicon_client_(std::move(favicon_client)),
      history_service_(history_service) {
  // TODO(crbug.com/40658964): convert to DCHECK once crash is resolved.
  CHECK(history_service_);
}

FaviconServiceImpl::~FaviconServiceImpl() = default;

base::CancelableTaskTracker::TaskId FaviconServiceImpl::GetFaviconImage(
    const GURL& icon_url,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "FaviconServiceImpl::GetFaviconImage");
  favicon_base::FaviconResultsCallback callback_runner = base::BindOnce(
      &FaviconServiceImpl::RunFaviconImageCallbackWithBitmapResults,
      std::move(callback), gfx::kFaviconSize);
  return history_service_->GetFavicon(
      icon_url, favicon_base::IconType::kFavicon,
      GetPixelSizesForFaviconScales(gfx::kFaviconSize),
      std::move(callback_runner), tracker);
}

base::CancelableTaskTracker::TaskId FaviconServiceImpl::GetRawFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    int desired_size_in_pixel,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "FaviconServiceImpl::GetRawFavicon");
  favicon_base::FaviconResultsCallback callback_runner =
      base::BindOnce(&favicon_base::ResizeFaviconBitmapResult,
                     desired_size_in_pixel)
          .Then(std::move(callback));

  std::vector<int> desired_sizes_in_pixel;
  desired_sizes_in_pixel.push_back(desired_size_in_pixel);

  return history_service_->GetFavicon(icon_url, icon_type,
                                      desired_sizes_in_pixel,
                                      std::move(callback_runner), tracker);
}

base::CancelableTaskTracker::TaskId FaviconServiceImpl::GetFavicon(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    int desired_size_in_dip,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "FaviconServiceImpl::GetFavicon");
  return history_service_->GetFavicon(
      icon_url, icon_type, GetPixelSizesForFaviconScales(desired_size_in_dip),
      std::move(callback), tracker);
}

base::CancelableTaskTracker::TaskId
FaviconServiceImpl::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "FaviconServiceImpl::GetFaviconImageForPageURL");
  return GetFaviconForPageURLImpl(
      page_url, {favicon_base::IconType::kFavicon},
      GetPixelSizesForFaviconScales(gfx::kFaviconSize),
      /*fallback_to_host=*/false,
      base::BindOnce(
          &FaviconServiceImpl::RunFaviconImageCallbackWithBitmapResults,
          std::move(callback), gfx::kFaviconSize),
      tracker);
}

base::CancelableTaskTracker::TaskId FaviconServiceImpl::GetRawFaviconForPageURL(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    int desired_size_in_pixel,
    bool fallback_to_host,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "FaviconServiceImpl::GetRawFaviconForPageURL");
  std::vector<int> desired_sizes_in_pixel;
  desired_sizes_in_pixel.push_back(desired_size_in_pixel);
  return GetFaviconForPageURLImpl(
      page_url, icon_types, desired_sizes_in_pixel, fallback_to_host,
      base::BindOnce(&favicon_base::ResizeFaviconBitmapResult,
                     desired_size_in_pixel)
          .Then(std::move(callback)),
      tracker);
}

base::CancelableTaskTracker::TaskId
FaviconServiceImpl::GetLargestRawFaviconForPageURL(
    const GURL& page_url,
    const std::vector<favicon_base::IconTypeSet>& icon_types,
    int minimum_size_in_pixels,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "FaviconServiceImpl::GetLargestRawFaviconForPageURL");
  if (favicon_client_ && favicon_client_->IsNativeApplicationURL(page_url)) {
    std::vector<int> desired_sizes_in_pixel;
    desired_sizes_in_pixel.push_back(0);
    return favicon_client_->GetFaviconForNativeApplicationURL(
        page_url, desired_sizes_in_pixel,
        base::BindOnce(&favicon_base::ResizeFaviconBitmapResult, 0)
            .Then(std::move(callback)),
        tracker);
  }
  const GURL fetched_url(
      (favicon_client_ && favicon_client_->IsReaderModeURL(page_url))
          ? favicon_client_->GetOriginalUrlFromReaderModeUrl(page_url)
          : page_url);
  return history_service_->GetLargestFaviconForURL(
      fetched_url, icon_types, minimum_size_in_pixels, std::move(callback),
      tracker);
}

base::CancelableTaskTracker::TaskId FaviconServiceImpl::GetFaviconForPageURL(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    int desired_size_in_dip,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "FaviconServiceImpl::GetFaviconForPageURL");
  return GetFaviconForPageURLImpl(
      page_url, icon_types, GetPixelSizesForFaviconScales(desired_size_in_dip),
      /*fallback_to_host=*/false, std::move(callback), tracker);
}

base::CancelableTaskTracker::TaskId
FaviconServiceImpl::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    int desired_size_in_dip,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  return history_service_->UpdateFaviconMappingsAndFetch(
      page_urls, icon_url, icon_type,
      GetPixelSizesForFaviconScales(desired_size_in_dip), std::move(callback),
      tracker);
}

void FaviconServiceImpl::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  return history_service_->DeleteFaviconMappings(page_urls, icon_type);
}

base::CancelableTaskTracker::TaskId
FaviconServiceImpl::GetLargestRawFaviconForID(
    favicon_base::FaviconID favicon_id,
    favicon_base::FaviconRawBitmapCallback callback,
    base::CancelableTaskTracker* tracker) {
  TRACE_EVENT0("browser", "FaviconServiceImpl::GetLargestRawFaviconForID");
  // Use 0 as `desired_size` to get the largest bitmap for `favicon_id` without
  // any resizing.
  int desired_size = 0;
  favicon_base::FaviconResultsCallback callback_runner =
      base::BindOnce(&favicon_base::ResizeFaviconBitmapResult, desired_size)
          .Then(std::move(callback));

  return history_service_->GetFaviconForID(favicon_id, desired_size,
                                           std::move(callback_runner), tracker);
}

void FaviconServiceImpl::SetFaviconOutOfDateForPage(const GURL& page_url) {
  history_service_->SetFaviconsOutOfDateForPage(page_url);
}

void FaviconServiceImpl::TouchOnDemandFavicon(const GURL& icon_url) {
  history_service_->TouchOnDemandFavicon(icon_url);
}

void FaviconServiceImpl::SetImportedFavicons(
    const favicon_base::FaviconUsageDataList& favicon_usage) {
  history_service_->SetImportedFavicons(favicon_usage);
}

void FaviconServiceImpl::AddPageNoVisitForBookmark(
    const GURL& url,
    const std::u16string& title) {
  history_service_->AddPageNoVisitForBookmark(url, title);
}

void FaviconServiceImpl::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  history_service_->MergeFavicon(page_url, icon_url, icon_type, bitmap_data,
                                 pixel_size);
}

void FaviconServiceImpl::SetFavicons(const base::flat_set<GURL>& page_urls,
                                     const GURL& icon_url,
                                     favicon_base::IconType icon_type,
                                     const gfx::Image& image) {
  history_service_->SetFavicons(page_urls, icon_type, icon_url,
                                ExtractSkBitmapsToStore(image));
}

void FaviconServiceImpl::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  history_service_->CloneFaviconMappingsForPages(page_url_to_read, icon_types,
                                                 page_urls_to_write);
}

void FaviconServiceImpl::CanSetOnDemandFavicons(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    base::OnceCallback<void(bool)> callback) const {
  history_service_->CanSetOnDemandFavicons(page_url, icon_type,
                                           std::move(callback));
}

void FaviconServiceImpl::SetOnDemandFavicons(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const gfx::Image& image,
    base::OnceCallback<void(bool)> callback) {
  history_service_->SetOnDemandFavicons(page_url, icon_type, icon_url,
                                        ExtractSkBitmapsToStore(image),
                                        std::move(callback));
}

void FaviconServiceImpl::UnableToDownloadFavicon(const GURL& icon_url) {
  MissingFaviconURLHash url_hash = base::FastHash(icon_url.spec());
  missing_favicon_urls_.insert(url_hash);
}

bool FaviconServiceImpl::WasUnableToDownloadFavicon(
    const GURL& icon_url) const {
  MissingFaviconURLHash url_hash = base::FastHash(icon_url.spec());
  return missing_favicon_urls_.find(url_hash) != missing_favicon_urls_.end();
}

void FaviconServiceImpl::ClearUnableToDownloadFavicons() {
  missing_favicon_urls_.clear();
}

base::CancelableTaskTracker::TaskId
FaviconServiceImpl::GetFaviconForPageURLImpl(
    const GURL& page_url,
    const favicon_base::IconTypeSet& icon_types,
    const std::vector<int>& desired_sizes_in_pixel,
    bool fallback_to_host,
    favicon_base::FaviconResultsCallback callback,
    base::CancelableTaskTracker* tracker) {
  if (favicon_client_ && favicon_client_->IsNativeApplicationURL(page_url)) {
    return favicon_client_->GetFaviconForNativeApplicationURL(
        page_url, desired_sizes_in_pixel, std::move(callback), tracker);
  }
  const GURL fetched_url(
      (favicon_client_ && favicon_client_->IsReaderModeURL(page_url))
          ? favicon_client_->GetOriginalUrlFromReaderModeUrl(page_url)
          : page_url);
  return history_service_->GetFaviconsForURL(
      fetched_url, icon_types, desired_sizes_in_pixel, fallback_to_host,
      std::move(callback), tracker);
}

}  // namespace favicon
