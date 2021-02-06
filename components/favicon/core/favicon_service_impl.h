// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_IMPL_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_usage_data.h"

class GURL;

namespace history {
class HistoryService;
}

namespace favicon {

class FaviconClient;

// The favicon service provides methods to access favicons. It calls the history
// backend behind the scenes. The callbacks are run asynchronously, even in the
// case of an error.
class FaviconServiceImpl : public FaviconService {
 public:
  // |history_service| most not be nullptr and  must outlive this object.
  FaviconServiceImpl(std::unique_ptr<FaviconClient> favicon_client,
                     history::HistoryService* history_service);
  ~FaviconServiceImpl() override;

  // FaviconService implementation.
  base::CancelableTaskTracker::TaskId GetFaviconImage(
      const GURL& icon_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId GetRawFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId GetFavicon(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId GetRawFaviconForPageURL(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      int desired_size_in_pixel,
      bool fallback_to_host,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<favicon_base::IconTypeSet>& icon_types,
      int minimum_size_in_pixels,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId GetFaviconForPageURL(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId UpdateFaviconMappingsAndFetch(
      const base::flat_set<GURL>& page_urls,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      int desired_size_in_dip,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;
  void DeleteFaviconMappings(const base::flat_set<GURL>& page_urls,
                             favicon_base::IconType icon_type) override;
  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForID(
      favicon_base::FaviconID favicon_id,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) override;
  void SetFaviconOutOfDateForPage(const GURL& page_url) override;
  void TouchOnDemandFavicon(const GURL& icon_url) override;
  void SetImportedFavicons(
      const favicon_base::FaviconUsageDataList& favicon_usage) override;
  void AddPageNoVisitForBookmark(const GURL& url,
                                 const base::string16& title) override;
  void MergeFavicon(const GURL& page_url,
                    const GURL& icon_url,
                    favicon_base::IconType icon_type,
                    scoped_refptr<base::RefCountedMemory> bitmap_data,
                    const gfx::Size& pixel_size) override;
  void SetFavicons(const base::flat_set<GURL>& page_urls,
                   const GURL& icon_url,
                   favicon_base::IconType icon_type,
                   const gfx::Image& image) override;
  void CloneFaviconMappingsForPages(
      const GURL& page_url_to_read,
      const favicon_base::IconTypeSet& icon_types,
      const base::flat_set<GURL>& page_urls_to_write) override;
  void CanSetOnDemandFavicons(
      const GURL& page_url,
      favicon_base::IconType icon_type,
      base::OnceCallback<void(bool)> callback) const override;
  void SetOnDemandFavicons(const GURL& page_url,
                           const GURL& icon_url,
                           favicon_base::IconType icon_type,
                           const gfx::Image& image,
                           base::OnceCallback<void(bool)> callback) override;
  void UnableToDownloadFavicon(const GURL& icon_url) override;
  bool WasUnableToDownloadFavicon(const GURL& icon_url) const override;
  void ClearUnableToDownloadFavicons() override;

 private:
  using MissingFaviconURLHash = size_t;

  // Helper function for GetFaviconImageForPageURL(), GetRawFaviconForPageURL()
  // and GetFaviconForPageURL().
  base::CancelableTaskTracker::TaskId GetFaviconForPageURLImpl(
      const GURL& page_url,
      const favicon_base::IconTypeSet& icon_types,
      const std::vector<int>& desired_sizes_in_pixel,
      bool fallback_to_host,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker);

  // Intermediate callback for GetFaviconImage() and GetFaviconImageForPageURL()
  // so that history service can deal solely with FaviconResultsCallback.
  // Builds favicon_base::FaviconImageResult from |favicon_bitmap_results| and
  // runs |callback|.
  void RunFaviconImageCallbackWithBitmapResults(
      favicon_base::FaviconImageCallback callback,
      int desired_size_in_dip,
      const std::vector<favicon_base::FaviconRawBitmapResult>&
          favicon_bitmap_results);

  // Intermediate callback for GetRawFavicon() and GetRawFaviconForPageURL()
  // so that history service can deal solely with FaviconResultsCallback.
  // Resizes favicon_base::FaviconRawBitmapResult if necessary and runs
  // |callback|.
  void RunFaviconRawBitmapCallbackWithBitmapResults(
      favicon_base::FaviconRawBitmapCallback callback,
      int desired_size_in_pixel,
      const std::vector<favicon_base::FaviconRawBitmapResult>&
          favicon_bitmap_results);

  std::unordered_set<MissingFaviconURLHash> missing_favicon_urls_;
  std::unique_ptr<FaviconClient> favicon_client_;
  history::HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(FaviconServiceImpl);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_SERVICE_IMPL_H_
