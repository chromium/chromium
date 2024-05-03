// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_
#define COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_

#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

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

// Keyed service for handling favicon requests made by a history UI, forwarding
// them to local storage or Google server accordingly. This service should
// only be used by the UIs listed in the HistoryUiFaviconRequestOrigin enum.
// Requests must be made by page url, as opposed to icon url.
class HistoryUiFaviconRequestHandler : public KeyedService {
 public:
  // Requests favicon bitmap at `page_url` of size `desired_size_in_pixel`.
  // Tries to fetch the icon from local storage and falls back to the Google
  // favicon server if user settings allow to query it using history data.
  virtual void GetRawFaviconForPageURL(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::FaviconRawBitmapCallback callback,
      HistoryUiFaviconRequestOrigin request_origin_for_uma) = 0;

  // Requests favicon image at `page_url`. The same fallback considerations for
  // GetRawFaviconForPageURL apply.
  // This method is only called by desktop code.
  virtual void GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      HistoryUiFaviconRequestOrigin request_origin_for_uma) = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_
