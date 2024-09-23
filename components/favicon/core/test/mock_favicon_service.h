// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_TEST_MOCK_FAVICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_TEST_MOCK_FAVICON_SERVICE_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace favicon {

class MockFaviconService : public FaviconService {
 public:
  MockFaviconService();
  ~MockFaviconService() override;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetFaviconImage,
              (const GURL& icon_url,
               favicon_base::FaviconImageCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetRawFavicon,
              (const GURL& icon_url,
               favicon_base::IconType icon_type,
               int desired_size_in_pixel,
               favicon_base::FaviconRawBitmapCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetFavicon,
              (const GURL& icon_url,
               favicon_base::IconType icon_type,
               int desired_size_in_dip,
               favicon_base::FaviconResultsCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetFaviconImageForPageURL,
              (const GURL& page_url,
               favicon_base::FaviconImageCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetRawFaviconForPageURL,
              (const GURL& page_url,
               const favicon_base::IconTypeSet& icon_types,
               int desired_size_in_pixel,
               bool fallback_to_host,
               favicon_base::FaviconRawBitmapCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargestRawFaviconForPageURL,
              (const GURL& page_url,
               const std::vector<favicon_base::IconTypeSet>& icon_types,
               int minimum_size_in_pixels,
               favicon_base::FaviconRawBitmapCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetFaviconForPageURL,
              (const GURL& page_url,
               const favicon_base::IconTypeSet& icon_types,
               int desired_size_in_dip,
               favicon_base::FaviconResultsCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              UpdateFaviconMappingsAndFetch,
              (const base::flat_set<GURL>& page_urls,
               const GURL& icon_url,
               favicon_base::IconType icon_type,
               int desired_size_in_dip,
               favicon_base::FaviconResultsCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(void,
              DeleteFaviconMappings,
              (const base::flat_set<GURL>& page_urls,
               favicon_base::IconType icon_type),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargestRawFaviconForID,
              (favicon_base::FaviconID favicon_id,
               favicon_base::FaviconRawBitmapCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(void,
              SetFaviconOutOfDateForPage,
              (const GURL& page_url),
              (override));
  MOCK_METHOD(void, TouchOnDemandFavicon, (const GURL& icon_url), (override));
  MOCK_METHOD(void,
              SetImportedFavicons,
              (const favicon_base::FaviconUsageDataList& favicon_usage),
              (override));
  MOCK_METHOD(void,
              AddPageNoVisitForBookmark,
              (const GURL& url, const std::u16string& title),
              (override));
  MOCK_METHOD(void,
              MergeFavicon,
              (const GURL& page_url,
               const GURL& icon_url,
               favicon_base::IconType icon_type,
               scoped_refptr<base::RefCountedMemory> bitmap_data,
               const gfx::Size& pixel_size),
              (override));
  MOCK_METHOD(void,
              SetFavicons,
              (const base::flat_set<GURL>& page_urls,
               const GURL& icon_url,
               favicon_base::IconType icon_type,
               const gfx::Image& image),
              (override));
  MOCK_METHOD(void,
              CloneFaviconMappingsForPages,
              (const GURL& page_url_to_read,
               const favicon_base::IconTypeSet& icon_types,
               const base::flat_set<GURL>& page_urls_to_write),
              (override));
  MOCK_METHOD(void,
              CanSetOnDemandFavicons,
              (const GURL& page_url,
               favicon_base::IconType icon_type,
               base::OnceCallback<void(bool)> callback),
              (const override));
  MOCK_METHOD(void,
              SetOnDemandFavicons,
              (const GURL& page_url,
               const GURL& icon_url,
               favicon_base::IconType icon_type,
               const gfx::Image& image,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(void,
              UnableToDownloadFavicon,
              (const GURL& icon_url),
              (override));
  MOCK_METHOD(bool,
              WasUnableToDownloadFavicon,
              (const GURL& icon_url),
              (const override));
  MOCK_METHOD(void, ClearUnableToDownloadFavicons, (), (override));
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_TEST_MOCK_FAVICON_SERVICE_H_
