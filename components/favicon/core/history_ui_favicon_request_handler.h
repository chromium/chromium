// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_
#define COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_

#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace favicon {

// Keyed service for handling favicon requests made by a history UI, forwarding
// them to local storage or Google server accordingly. Requests must be made by
// page url, as opposed to icon url.
class HistoryUiFaviconRequestHandler : public KeyedService {
 public:
  // Requests favicon bitmap at `page_url` of size `desired_size_in_pixel`.
  // Tries to fetch the icon from local storage and falls back to the Google
  // favicon server if user settings allow to query it using history data.
  // `fallback_to_host` enables the local storage fuzzy matching fallback.
  virtual void GetRawFaviconForPageURL(
      const GURL& page_url,
      int desired_size_in_pixel,
      bool fallback_to_host,
      favicon_base::FaviconRawBitmapCallback callback) = 0;

  // Requests favicon image at `page_url`. The same fallback considerations for
  // GetRawFaviconForPageURL apply.
  // This method is only called by desktop code.
  virtual void GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback) = 0;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_HISTORY_UI_FAVICON_REQUEST_HANDLER_H_
