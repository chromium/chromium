// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_UTILS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_UTILS_H_

#include <vector>

#include "base/functional/callback.h"
#include "ui/base/mojom/window_open_disposition.mojom-forward.h"

class BrowserWindowInterface;
class GURL;

namespace content {
class NavigationHandle;
}  // namespace content

namespace bookmarks {

// Opens the bookmarked `url` from `browser` with `disposition`. `callback` runs
// with the navigation once it starts.
void OpenBookmarkUrl(
    BrowserWindowInterface* browser,
    const GURL& url,
    ui::mojom::WindowOpenDisposition disposition,
    base::OnceCallback<void(content::NavigationHandle&)> callback = {});

// Opens the bookmarked `urls` from `browser` with `disposition`. For a new
// window, they all open in the same new window.
void OpenBookmarkUrls(BrowserWindowInterface* browser,
                      const std::vector<GURL>& urls,
                      ui::mojom::WindowOpenDisposition disposition);

}  // namespace bookmarks

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_ANDROID_ANDROID_BOOKMARK_UTILS_H_
