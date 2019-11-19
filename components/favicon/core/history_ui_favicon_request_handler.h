// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_
#define COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_

#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class CancelableTaskTracker;
}

class GURL;

namespace favicon {

// The UI origin of an icon request. Used to do metrics recording per UI.
enum class HistoryUiFaviconRequestOrigin {
  // History page.
  kHistory,
  // History synced tabs page (desktop only).
  kHistorySyncedTabs,
  // Recent tabs user interface.
  kRecentTabs,
};

// Platform making the request.
enum class FaviconRequestPlatform {
  kMobile,
  kDesktop,
};

// Keyed service for handling favicon requests made by a history UI, forwarding
// them to local storage, sync or Google server accordingly. This service should
// only be used by the UIs listed in the HistoryUiFaviconRequestOrigin enum.
// Requests must be made by page url, as opposed to icon url.
class HistoryUiFaviconRequestHandler : public KeyedService {
 public:
  // Requests favicon bitmap at |page_url| of size |desired_size_in_pixel|.
  // Tries to fetch the icon from local storage and falls back to sync, or to
  // Google favicon server if |favicon::kEnableHistoryFaviconsGoogleServerQuery|
  // is enabled. If a non-empty |icon_url_for_uma| (optional) is passed, it will
  // be used to record UMA about the grouping of requests to the favicon server.
  // |request_platform| specifies whether the caller is mobile or desktop code.
  // TODO(victorvianna): Remove platform parameter since it's no longer needed.
  virtual void GetRawFaviconForPageURL(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback,
      FaviconRequestPlatform request_platform,
      HistoryUiFaviconRequestOrigin request_origin_for_uma,
      const GURL& icon_url_for_uma,
      base::CancelableTaskTracker* tracker) = 0;

  // Requests favicon image at |page_url|.
  // Tries to fetch the icon from local storage and falls back to sync, or to
  // Google favicon server if |favicon::kEnableHistoryFaviconsGoogleServerQuery|
  // is enabled.
  // If a non-empty |icon_url_for_uma| (optional) is passed, it will be used to
  // record UMA about the grouping of requests to the favicon server.
  // This method is only called by desktop code.
  virtual void GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      HistoryUiFaviconRequestOrigin request_origin_for_uma,
      const GURL& icon_url_for_uma,
      base::CancelableTaskTracker* tracker) = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_
