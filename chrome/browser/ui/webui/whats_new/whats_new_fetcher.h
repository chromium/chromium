// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_FETCHER_H_

#include "components/user_education/webui/whats_new_registry.h"
#include "url/gurl.h"

class Browser;

namespace whats_new {

extern const char kChromeWhatsNewV2URL[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LoadEvent {
  kLoadStart = 0,
  kLoadSuccess = 1,
  kLoadFailAndShowError = 2,
  kLoadFailAndFallbackToNtp = 3,
  kLoadFailAndCloseTab = 4,
  kLoadFailAndDoNotShow = 5,
  kLoadAbort = 6,
  kMaxValue = kLoadAbort,
};

// Whats New V2 API
// Gets the server side URL for the What's New page for the current version
// of Chrome.
GURL GetV2ServerURL(bool is_staging = false);

// Whats New V2 API
// Gets the server side URL for the What's New page including all
// query parameters necessary to render the page.
GURL GetV2ServerURLForRender(
    const whats_new::WhatsNewRegistry& whats_new_registry,
    bool is_staging = false);

// Starts fetching the What's New page and will open the page in |browser| if
// it exists.
void StartWhatsNewFetch(Browser* browser);

}  // namespace whats_new

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_FETCHER_H_
