// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_handler.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/features.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

namespace favicon {
namespace {

const int kLargestIconSize = 192;

// Return true if |bitmap_result| is expired.
bool IsExpired(const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  return bitmap_result.expired;
}

// Return true if |bitmap_result| is valid.
bool IsValid(const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  return bitmap_result.is_valid();
}

// Returns true if |bitmap_results| is non-empty and:
// - At least one of the bitmaps in |bitmap_results| is expired
// OR
// - |bitmap_results| is missing favicons for |desired_size_in_dip| and one of
//   the scale factors in favicon_base::GetFaviconScales().
bool HasExpiredOrIncompleteResult(
    int desired_size_in_dip,
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  if (bitmap_results.empty())
    return false;

  // Check if at least one of the bitmaps is expired.
  auto it =
      std::find_if(bitmap_results.begin(), bitmap_results.end(), IsExpired);
  if (it != bitmap_results.end())
    return true;

  // Any favicon size is good if the desired size is 0.
  if (desired_size_in_dip == 0)
    return false;

  // Check if the favicon for at least one of the scale factors is missing.
  // |bitmap_results| should always be complete for data inserted by
  // FaviconHandler as the FaviconHandler stores favicons resized to all
  // of favicon_base::GetFaviconScales() into the history backend.
  // Examples of when |bitmap_results| can be incomplete:
  // - Favicons inserted into the history backend by sync.
  // - Favicons for imported bookmarks.
  std::vector<gfx::Size> favicon_sizes;
  for (const auto& bitmap_result : bitmap_results)
    favicon_sizes.push_back(bitmap_result.pixel_size);

  std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  for (float favicon_scale : favicon_scales) {
    int edge_size_in_pixel = std::ceil(desired_size_in_dip * favicon_scale);
    gfx::Size value(edge_size_in_pixel, edge_size_in_pixel);
    if (!base::Contains(favicon_sizes, value))
      return true;
  }
  return false;
}

// Returns true if at least one of |bitmap_results| is valid.
bool HasValidResult(
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  return std::find_if(bitmap_results.begin(), bitmap_results.end(), IsValid) !=
      bitmap_results.end();
}

std::vector<int> GetDesiredPixelSizes(
    FaviconDriverObserver::NotificationIconType handler_type) {
  switch (handler_type) {
    case FaviconDriverObserver::NON_TOUCH_16_DIP: {
      std::vector<int> pixel_sizes;
      for (float scale_factor : favicon_base::GetFaviconScales()) {
        pixel_sizes.push_back(
            static_cast<int>(ceil(scale_factor * gfx::kFaviconSize)));
      }
      return pixel_sizes;
    }
    case FaviconDriverObserver::NON_TOUCH_LARGEST:
    case FaviconDriverObserver::TOUCH_LARGEST:
      return std::vector<int>(1U, kLargestIconSize);
  }
  NOTREACHED();
  return std::vector<int>();
}

bool FaviconURLEquals(const FaviconURL& lhs, const FaviconURL& rhs) {
  return lhs.icon_url == rhs.icon_url && lhs.icon_type == rhs.icon_type &&
         lhs.icon_sizes == rhs.icon_sizes;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// static
FaviconHandler::FaviconCandidate
FaviconHandler::FaviconCandidate::FromFaviconURL(
    const favicon::FaviconURL& favicon_url,
    const std::vector<int>& desired_pixel_sizes,
    bool want_largest_icon) {
  FaviconCandidate candidate;
  candidate.icon_url = favicon_url.icon_url;
  candidate.icon_type = favicon_url.icon_type;

  if (!favicon_url.icon_sizes.empty()) {
    // For candidates with explicit size information, the score is computed
    // based on similarity with |desired_pixel_sizes|.
    SelectFaviconFrameIndices(favicon_url.icon_sizes, desired_pixel_sizes,
                              /*best_indices=*/nullptr, &candidate.score);
  } else if (want_largest_icon) {
    // If looking for largest icon (mobile), candidates without explicit size
    // information are scored low because they are likely small.
    candidate.score = 0.0f;
  } else {
    // If looking for small icons (desktop), candidates without explicit size
    // information are scored highest, as high as candidates with an ideal
    // explicit size information. This guarantees all candidates without
    // explicit size information will be processed until an ideal candidate is
    // found (if available).
    candidate.score = 1.0f;
  }

  return candidate;
}

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::FaviconHandler(
    FaviconService* service,
    Delegate* delegate,
    FaviconDriverObserver::NotificationIconType handler_type)
    : handler_type_(handler_type),
      got_favicon_from_history_(false),
      initial_history_result_expired_or_incomplete_(false),
      redownload_icons_(false),
      icon_types_(FaviconHandler::GetIconTypesFromHandlerType(handler_type)),
      download_largest_icon_(
          handler_type == FaviconDriverObserver::NON_TOUCH_LARGEST ||
          handler_type == FaviconDriverObserver::TOUCH_LARGEST),
      candidates_received_(false),
      error_other_than_404_found_(false),
      notification_icon_type_(favicon_base::IconType::kInvalid),
      service_(service),
      delegate_(delegate),
      current_candidate_index_(0u) {
  DCHECK(delegate_);
}

FaviconHandler::~FaviconHandler() {
}

// static
favicon_base::IconTypeSet FaviconHandler::GetIconTypesFromHandlerType(
    FaviconDriverObserver::NotificationIconType handler_type) {
  switch (handler_type) {
    case FaviconDriverObserver::NON_TOUCH_16_DIP:
    case FaviconDriverObserver::NON_TOUCH_LARGEST:
      return {favicon_base::IconType::kFavicon};
    case FaviconDriverObserver::TOUCH_LARGEST:
      return {favicon_base::IconType::kTouchIcon,
              favicon_base::IconType::kTouchPrecomposedIcon,
              favicon_base::IconType::kWebManifestIcon};
  }
  return {};
}

void FaviconHandler::FetchFavicon(const GURL& page_url, bool is_same_document) {
  cancelable_task_tracker_for_page_url_.TryCancelAll();
  cancelable_task_tracker_for_candidates_.TryCancelAll();

  // We generally clear |page_urls_| and start clean unless there are obvious
  // reasons to think URLs share favicons: the navigation must be within the
  // same document (e.g. fragment navigation) AND it happened so early that no
  // candidates were received for the previous URL(s) (e.g. redirect-like
  // history.replaceState() during page load).
  if (!is_same_document || candidates_received_) {
    page_urls_.clear();
  }
  page_urls_.insert(page_url);
  last_page_url_ = page_url;

  initial_history_result_expired_or_incomplete_ = false;
  redownload_icons_ = false;
  got_favicon_from_history_ = false;
  manifest_download_request_.Cancel();
  image_download_request_.Cancel();
  candidates_received_ = false;
  manifest_url_ = GURL();
  non_manifest_original_candidates_.clear();
  final_candidates_.reset();
  notification_icon_url_ = GURL();
  notification_icon_type_ = favicon_base::IconType::kInvalid;
  current_candidate_index_ = 0u;
  best_favicon_ = DownloadedFavicon();

  // Request the favicon from the history service. In parallel to this the
  // renderer is going to notify us (well WebContents) when the favicon url is
  // available. We use |last_page_url_| specifically (regardless of other
  // possible values in |page_urls_|) because we want to use the most
  // up-to-date / latest URL for DB lookups, which is the page URL for which
  // we get <link rel="icon"> candidates (FaviconHandler::OnUpdateCandidates()).
  service_->GetFaviconForPageURL(
      last_page_url_, icon_types_, preferred_icon_size(),
      base::BindOnce(
          &FaviconHandler::OnFaviconDataForInitialURLFromFaviconService,
          base::Unretained(this)),
      &cancelable_task_tracker_for_page_url_);
}

bool FaviconHandler::ShouldDownloadNextCandidate() const {
  // Stop downloading if the current candidate is the last candidate.
  if (current_candidate_index_ + 1 >= final_candidates_->size())
    return false;

  // Continue downloading if no valid favicon has been downloaded yet.
  if (best_favicon_.candidate.icon_type == favicon_base::IconType::kInvalid)
    return true;

  // |next_candidate_score| is based on the sizes provided in the <link> tag,
  // see FaviconCandidate::FromFaviconURL().
  float next_candidate_score =
      (*final_candidates_)[current_candidate_index_ + 1].score;

  // Continue downloading only if the next candidate is better than the best one
  // observed so far.
  return next_candidate_score > best_favicon_.candidate.score;
}

void FaviconHandler::SetFavicon(const GURL& icon_url,
                                const gfx::Image& image,
                                favicon_base::IconType icon_type) {
  // Associate the icon to all URLs in |page_urls_|, which contains page URLs
  // within the same site/document that have been considered to reliably share
  // the same icon candidates.
  if (!delegate_->IsOffTheRecord())
    service_->SetFavicons(page_urls_, icon_url, icon_type, image);

  NotifyFaviconUpdated(icon_url, icon_type, image);
}

void FaviconHandler::MaybeDeleteFaviconMappings() {
  DCHECK(candidates_received_);
  DCHECK(got_favicon_from_history_);

  // The order of these conditions is important because we want the feature
  // state to be checked at the very end.
  if (!error_other_than_404_found_ &&
      notification_icon_type_ != favicon_base::IconType::kInvalid) {
    if (!delegate_->IsOffTheRecord())
      service_->DeleteFaviconMappings(page_urls_, notification_icon_type_);

    delegate_->OnFaviconDeleted(last_page_url_, handler_type_);

    notification_icon_url_ = GURL();
    notification_icon_type_ = favicon_base::IconType::kInvalid;
  }
}

void FaviconHandler::NotifyFaviconUpdated(
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results) {
  DCHECK(!favicon_bitmap_results.empty());

  gfx::Image resized_image = favicon_base::SelectFaviconFramesFromPNGs(
      favicon_bitmap_results,
      favicon_base::GetFaviconScales(),
      preferred_icon_size());
  // The history service sends back results for a single icon URL and icon
  // type, so it does not matter which result we get |icon_url| and |icon_type|
  // from.
  const GURL icon_url = favicon_bitmap_results[0].icon_url;
  favicon_base::IconType icon_type = favicon_bitmap_results[0].icon_type;
  NotifyFaviconUpdated(icon_url, icon_type, resized_image);
}

void FaviconHandler::NotifyFaviconUpdated(const GURL& icon_url,
                                          favicon_base::IconType icon_type,
                                          const gfx::Image& image) {
  if (image.IsEmpty())
    return;

  gfx::Image image_with_adjusted_colorspace = image;
  favicon_base::SetFaviconColorSpace(&image_with_adjusted_colorspace);

  delegate_->OnFaviconUpdated(last_page_url_, handler_type_, icon_url,
                              icon_url != notification_icon_url_,
                              image_with_adjusted_colorspace);

  notification_icon_url_ = icon_url;
  notification_icon_type_ = icon_type;
}

void FaviconHandler::OnUpdateCandidates(
    const GURL& page_url,
    const std::vector<FaviconURL>& candidates,
    const GURL& manifest_url) {
  if (last_page_url_ != page_url)
    return;

  // |candidates| or |manifest_url| could have been modified via Javascript. If
  // neither changed, ignore the call.
  if (candidates_received_ && manifest_url_ == manifest_url &&
      (non_manifest_original_candidates_.size() == candidates.size() &&
       std::equal(candidates.begin(), candidates.end(),
                  non_manifest_original_candidates_.begin(),
                  &FaviconURLEquals))) {
    return;
  }

  candidates_received_ = true;
  error_other_than_404_found_ = false;
  non_manifest_original_candidates_ = candidates;
  final_candidates_.reset();
  cancelable_task_tracker_for_candidates_.TryCancelAll();
  manifest_download_request_.Cancel();
  image_download_request_.Cancel();
  current_candidate_index_ = 0u;
  best_favicon_ = DownloadedFavicon();
  manifest_url_ = manifest_url;

  // Check if the manifest was previously blacklisted (e.g. returned a 404) and
  // ignore the manifest URL if that's the case.
  if (!manifest_url_.is_empty() &&
      service_->WasUnableToDownloadFavicon(manifest_url_)) {
    DVLOG(1) << "Skip failed Manifest: " << manifest_url;
    manifest_url_ = GURL();
  }

  // If no manifest available, proceed with the regular candidates only.
  if (manifest_url_.is_empty()) {
    OnGotFinalIconURLCandidates(candidates);
    return;
  }

  // See if there is a cached favicon for the manifest. This will update the DB
  // mappings only if the manifest URL is cached.
  GetFaviconAndUpdateMappingsUnlessIncognito(
      /*icon_url=*/manifest_url_, favicon_base::IconType::kWebManifestIcon,
      base::BindOnce(
          &FaviconHandler::OnFaviconDataForManifestFromFaviconService,
          base::Unretained(this)));
}

void FaviconHandler::OnFaviconDataForManifestFromFaviconService(
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results) {
  // The database lookup for the page URL is guaranteed to be completed because
  // the HistoryBackend uses a SequencedTaskRunner, and we also know that
  // FetchFavicon() was called before OnUpdateCandidates().
  DCHECK(got_favicon_from_history_);

  bool has_valid_result = HasValidResult(favicon_bitmap_results);
  bool has_expired_or_incomplete_result =
      !has_valid_result || HasExpiredOrIncompleteResult(preferred_icon_size(),
                                                        favicon_bitmap_results);

  if (has_valid_result &&
      (notification_icon_url_ != manifest_url_ ||
       notification_icon_type_ != favicon_base::IconType::kWebManifestIcon)) {
    // There is a valid favicon. Notify any observers. It is useful to notify
    // the observers even if the favicon is expired or incomplete (incorrect
    // size) because temporarily showing the user an expired favicon or
    // streched favicon is preferable to showing the user the default favicon.
    NotifyFaviconUpdated(favicon_bitmap_results);
  }

  if (has_expired_or_incomplete_result) {
    manifest_download_request_.Reset(base::BindOnce(
        &FaviconHandler::OnDidDownloadManifest, base::Unretained(this)));
    delegate_->DownloadManifest(manifest_url_,
                                manifest_download_request_.callback());
  }
}

void FaviconHandler::OnDidDownloadManifest(
    const std::vector<FaviconURL>& candidates) {
  // Mark manifest download as finished.
  manifest_download_request_.Cancel();

  if (!candidates.empty()) {
    OnGotFinalIconURLCandidates(candidates);
    return;
  }

  // If either the downloading of the manifest failed, OR the manifest contains
  // no icons, proceed with the list of icons listed in the HTML.
  DVLOG(1) << "Could not fetch Manifest icons from " << manifest_url_
           << ", falling back to inlined ones, which are "
           << non_manifest_original_candidates_.size();

  service_->UnableToDownloadFavicon(manifest_url_);
  manifest_url_ = GURL();

  OnGotFinalIconURLCandidates(non_manifest_original_candidates_);
}

void FaviconHandler::OnGotFinalIconURLCandidates(
    const std::vector<FaviconURL>& candidates) {
  DCHECK(!final_candidates_);

  const std::vector<int> desired_pixel_sizes =
      GetDesiredPixelSizes(handler_type_);

  std::vector<FaviconCandidate> sorted_candidates;
  for (const FaviconURL& candidate : candidates) {
    if (!candidate.icon_url.is_empty() &&
        (icon_types_.count(candidate.icon_type) != 0)) {
      sorted_candidates.push_back(FaviconCandidate::FromFaviconURL(
          candidate, desired_pixel_sizes, download_largest_icon_));
    }
  }

  std::stable_sort(sorted_candidates.begin(), sorted_candidates.end(),
                   &FaviconCandidate::CompareScore);

  final_candidates_ = std::move(sorted_candidates);

  if (got_favicon_from_history_)
    OnGotInitialHistoryDataAndIconURLCandidates();
}

// static
int FaviconHandler::GetMaximalIconSize(
    FaviconDriverObserver::NotificationIconType handler_type,
    bool candidates_from_web_manifest) {
  int max_size = 0;
  for (int size : GetDesiredPixelSizes(handler_type)) {
    max_size = std::max(max_size, size);
  }
  return max_size;
}

void FaviconHandler::OnGotInitialHistoryDataAndIconURLCandidates() {
  DCHECK(final_candidates_);
  DCHECK(got_favicon_from_history_);
  DCHECK_EQ(0U, current_candidate_index_);

  if (final_candidates_->empty()) {
    // The page lists no candidates that match our target |icon_types_|, so
    // check if any existing mappings should be deleted.
    MaybeDeleteFaviconMappings();
    return;
  }

  if (!initial_history_result_expired_or_incomplete_ &&
      current_candidate()->icon_url == notification_icon_url_ &&
      current_candidate()->icon_type == notification_icon_type_) {
    // - The data from history is valid and not expired.
    // - The icon URL of the history data matches one of the page's icon URLs.
    // - The icon URL of the history data matches the icon URL of the last
    //   OnFaviconAvailable() notification.
    // We are done. No additional downloads or history requests are needed.
    // TODO: Store all of the icon URLs associated with a page in history so
    // that we can check whether the page's icon URLs match the page's icon URLs
    // at the time that the favicon data was stored to the history database.
    return;
  }

  DownloadCurrentCandidateOrAskFaviconService();
}

void FaviconHandler::OnDidDownloadFavicon(
    favicon_base::IconType icon_type,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  // Mark download as finished.
  image_download_request_.Cancel();

  if (bitmaps.empty()) {
    if (http_status_code == 404) {
      DVLOG(1) << "Failed to Download Favicon:" << image_url;
      service_->UnableToDownloadFavicon(image_url);
    } else if (http_status_code != 0) {
      error_other_than_404_found_ = true;
    }
  } else {
    float score = 0.0f;
    gfx::ImageSkia image_skia;
    if (download_largest_icon_) {
      std::vector<size_t> best_indices;
      SelectFaviconFrameIndices(original_bitmap_sizes,
                                GetDesiredPixelSizes(handler_type_),
                                &best_indices, &score);
      DCHECK_EQ(1U, best_indices.size());
      image_skia =
          gfx::ImageSkia::CreateFrom1xBitmap(bitmaps[best_indices.front()]);
    } else {
      image_skia = CreateFaviconImageSkia(bitmaps,
                                          original_bitmap_sizes,
                                          preferred_icon_size(),
                                          &score);
    }

    if (!image_skia.isNull() && score > best_favicon_.candidate.score) {
      best_favicon_.image = gfx::Image(image_skia);
      best_favicon_.candidate.icon_url = image_url;
      best_favicon_.candidate.icon_type = icon_type;
      best_favicon_.candidate.score = score;
    }
  }

  if (ShouldDownloadNextCandidate()) {
    // Process the next candidate.
    ++current_candidate_index_;
    DCHECK_LT(current_candidate_index_, final_candidates_->size());
    DownloadCurrentCandidateOrAskFaviconService();
  } else {
    if (best_favicon_.candidate.icon_type == favicon_base::IconType::kInvalid) {
      // No valid icon found, so check if mappings should be deleted.
      MaybeDeleteFaviconMappings();
    } else {
      // We have either found the ideal candidate or run out of candidates.
      // No more icons to request, set the favicon from the candidate. The
      // manifest URL, if available, is used instead of the icon URL.
      SetFavicon(manifest_url_.is_empty() ? best_favicon_.candidate.icon_url
                                          : manifest_url_,
                 best_favicon_.image,
                 manifest_url_.is_empty()
                     ? best_favicon_.candidate.icon_type
                     : favicon_base::IconType::kWebManifestIcon);
    }
    // Clear download related state.
    current_candidate_index_ = final_candidates_->size();
    best_favicon_ = DownloadedFavicon();
  }
}

const std::vector<GURL> FaviconHandler::GetIconURLs() const {
  std::vector<GURL> icon_urls;
  for (const FaviconCandidate& candidate : *final_candidates_)
    icon_urls.push_back(candidate.icon_url);
  return icon_urls;
}

bool FaviconHandler::HasPendingTasksForTest() {
  return !image_download_request_.IsCancelled() ||
         !manifest_download_request_.IsCancelled() ||
         cancelable_task_tracker_for_page_url_.HasTrackedTasks() ||
         cancelable_task_tracker_for_candidates_.HasTrackedTasks();
}

void FaviconHandler::OnFaviconDataForInitialURLFromFaviconService(
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results) {
  got_favicon_from_history_ = true;
  bool has_valid_result = HasValidResult(favicon_bitmap_results);
  initial_history_result_expired_or_incomplete_ =
      !has_valid_result ||
      HasExpiredOrIncompleteResult(preferred_icon_size(),
                                   favicon_bitmap_results);
  redownload_icons_ = initial_history_result_expired_or_incomplete_ &&
                      !favicon_bitmap_results.empty();

  if (has_valid_result) {
    // Propagate mappings to all redirects, in case the redirect chain is
    // different from the one observed the previous time the page URL was
    // visited.
    //
    // Do the propagation now because we want the propagation to occur in all
    // scenarios and this is an easy way of guaranteeing it. For instance, we
    // want the propagation to occur when:
    // - The favicon in the database is expired.
    // AND
    // - Redownloading the favicon fails with a non-404 error code.
    if (!delegate_->IsOffTheRecord() &&
        base::FeatureList::IsEnabled(kAllowPropagationOfFaviconCacheHits)) {
      service_->CloneFaviconMappingsForPages(last_page_url_, icon_types_,
                                             page_urls_);
    }

    // The db knows the favicon (although it may be out of date). Set the
    // favicon now, and if the favicon turns out to be expired (or the wrong
    // url) we'll fetch later on. This way the user doesn't see a flash of the
    // default favicon.
    NotifyFaviconUpdated(favicon_bitmap_results);
  }

  if (final_candidates_)
    OnGotInitialHistoryDataAndIconURLCandidates();
}

void FaviconHandler::DownloadCurrentCandidateOrAskFaviconService() {
  DCHECK(image_download_request_.IsCancelled());
  DCHECK(manifest_download_request_.IsCancelled());
  DCHECK(current_candidate());

  const GURL icon_url = current_candidate()->icon_url;
  const favicon_base::IconType icon_type = current_candidate()->icon_type;
  // If the icons listed in a manifest are being processed, skip the cache
  // lookup for |icon_url| since the manifest's URL is used for caching, not the
  // icon URL, and this lookup has happened earlier.
  if (redownload_icons_ || !manifest_url_.is_empty()) {
    // We have the mapping, but the favicon is out of date. Download it now.
    ScheduleImageDownload(icon_url, icon_type);
  } else {
    GetFaviconAndUpdateMappingsUnlessIncognito(
        icon_url, icon_type,
        base::BindOnce(&FaviconHandler::OnFaviconData, base::Unretained(this)));
  }
}

void FaviconHandler::GetFaviconAndUpdateMappingsUnlessIncognito(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    favicon_base::FaviconResultsCallback callback) {
  // We don't know the favicon, but we may have previously downloaded the
  // favicon for another page that shares the same favicon. Ask for the
  // favicon given the favicon URL.
  if (delegate_->IsOffTheRecord()) {
    service_->GetFavicon(icon_url, icon_type, preferred_icon_size(),
                         std::move(callback),
                         &cancelable_task_tracker_for_candidates_);
  } else {
    // Ask the history service for the icon. This does two things:
    // 1. Attempts to fetch the favicon data from the database.
    // 2. If the favicon exists in the database, this updates the database to
    //    include the mapping between the page url and the favicon url.
    // This is asynchronous. The history service will call back when done.
    service_->UpdateFaviconMappingsAndFetch(
        page_urls_, icon_url, icon_type, preferred_icon_size(),
        std::move(callback), &cancelable_task_tracker_for_candidates_);
  }
}

void FaviconHandler::OnFaviconData(const std::vector<
    favicon_base::FaviconRawBitmapResult>& favicon_bitmap_results) {
  bool has_valid_result = HasValidResult(favicon_bitmap_results);
  bool has_expired_or_incomplete_result =
      !has_valid_result || HasExpiredOrIncompleteResult(preferred_icon_size(),
                                                        favicon_bitmap_results);

  if (has_valid_result) {
    // There is a valid favicon. Notify any observers. It is useful to notify
    // the observers even if the favicon is expired or incomplete (incorrect
    // size) because temporarily showing the user an expired favicon or
    // streched favicon is preferable to showing the user the default favicon.
    NotifyFaviconUpdated(favicon_bitmap_results);
  }

  if (has_expired_or_incomplete_result) {
    ScheduleImageDownload(current_candidate()->icon_url,
                          current_candidate()->icon_type);
  }
}

void FaviconHandler::ScheduleImageDownload(const GURL& image_url,
                                           favicon_base::IconType icon_type) {
  DCHECK(image_url.is_valid());
  // Note that CancelableCallback starts cancelled.
  DCHECK(image_download_request_.IsCancelled())
      << "More than one ongoing download";
  if (service_->WasUnableToDownloadFavicon(image_url)) {
    DVLOG(1) << "Skip Failed FavIcon: " << image_url;
    OnDidDownloadFavicon(icon_type, 0, 0, image_url, std::vector<SkBitmap>(),
                         std::vector<gfx::Size>());
    return;
  }
  image_download_request_.Reset(
      base::BindOnce(&FaviconHandler::OnDidDownloadFavicon,
                     base::Unretained(this), icon_type));
  // A max bitmap size is specified to avoid receiving huge bitmaps in
  // OnDidDownloadFavicon(). See FaviconDriver::StartDownload()
  // for more details about the max bitmap size.
  const int download_id = delegate_->DownloadImage(
      image_url, GetMaximalIconSize(handler_type_, !manifest_url_.is_empty()),
      image_download_request_.callback());
  DCHECK_NE(download_id, 0);
}

}  // namespace favicon
