// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_TEST_MOCK_LARGE_ICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_TEST_MOCK_LARGE_ICON_SERVICE_H_

#include <optional>

#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace favicon {

class MockLargeIconService : public LargeIconService {
 public:
  MockLargeIconService();
  MockLargeIconService(const MockLargeIconService&) = delete;
  MockLargeIconService& operator=(const MockLargeIconService&) = delete;
  ~MockLargeIconService() override;

  MOCK_METHOD(void,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
              (const GURL& page_url,
               bool should_trim_page_url_path,
               const net::NetworkTrafficAnnotationTag& traffic_annotation,
               favicon_base::GoogleFaviconServerCallback callback),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconImageOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconImageCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapForPageUrl,
              (const GURL& page_url,
               int min_source_size_in_pixel,
               std::optional<int> size_in_pixel_to_resize_to,
               NoBigEnoughIconBehavior no_big_enough_icon_behavior,
               NonSquareIconBehavior non_square_icon_behavior,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
              (const GURL& icon_url,
               int min_source_size_in_pixel,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              GetIconRawBitmapOrFallbackStyleForPageUrl,
              (const GURL& page_url,
               int desired_size_in_pixel,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(void,
              GetLargeIconFromCacheFallbackToGoogleServer,
              (const GURL& page_url,
               StandardIconSize min_source_size_in_pixel,
               std::optional<StandardIconSize> size_in_pixel_to_resize_to,
               NoBigEnoughIconBehavior no_big_enough_icon_behavior,
               bool should_trim_page_url_path,
               const net::NetworkTrafficAnnotationTag& traffic_annotation,
               favicon_base::LargeIconCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
  MOCK_METHOD(void,
              TouchIconFromGoogleServer,
              (const GURL& icon_url),
              (override));
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_TEST_MOCK_LARGE_ICON_SERVICE_H_
