// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_utils.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace bookmarks {

namespace {

using ui::mojom::WindowOpenDisposition;

// Opens the first URL in a new window, then the rest as background tabs in that
// window. The `OpenURL()` callback can run before the new window is ready, so
// wait for `Navigate()`'s callback, which runs once it is.
void OpenBookmarkUrlsInNewWindow(BrowserWindowInterface* browser,
                                 const std::vector<GURL>& urls,
                                 WindowOpenDisposition disposition) {
  auto params = std::make_unique<NavigateParams>(
      browser, urls.front(), ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params->disposition = disposition;
  params->window_action = NavigateParams::WindowAction::kShowWindow;
  NavigateParams* raw_params = params.get();
  Navigate(raw_params,
           base::BindOnce(
               [](std::unique_ptr<NavigateParams> params,
                  base::WeakPtr<BrowserWindowInterface> original_browser,
                  std::vector<GURL> urls_to_open,
                  base::WeakPtr<content::NavigationHandle>) {
                 // `params->browser` is left unchanged if the new window
                 // couldn't be created.
                 BrowserWindowInterface* new_browser = params->browser;
                 if (!new_browser || new_browser == original_browser.get()) {
                   return;
                 }
                 for (const GURL& url : urls_to_open) {
                   OpenBookmarkUrl(new_browser, url,
                                   WindowOpenDisposition::kNewBackgroundTab);
                 }
               },
               std::move(params), browser->GetWeakPtr(),
               std::vector<GURL>(urls.begin() + 1, urls.end())));
}

}  // namespace

void OpenBookmarkUrl(
    BrowserWindowInterface* browser,
    const GURL& url,
    WindowOpenDisposition disposition,
    base::OnceCallback<void(content::NavigationHandle&)> callback) {
  browser->OpenURL(content::OpenURLParams::CreateBrowserInitiated(
                       url, disposition, ui::PAGE_TRANSITION_AUTO_BOOKMARK),
                   std::move(callback));
}

void OpenBookmarkUrls(BrowserWindowInterface* browser,
                      const std::vector<GURL>& urls,
                      WindowOpenDisposition disposition) {
  const bool opens_in_new_window =
      disposition == WindowOpenDisposition::kNewWindow ||
      disposition == WindowOpenDisposition::kOffTheRecord;
  if (opens_in_new_window && urls.size() > 1) {
    OpenBookmarkUrlsInNewWindow(browser, urls, disposition);
    return;
  }
  for (const GURL& url : urls) {
    OpenBookmarkUrl(browser, url, disposition);
  }
}

}  // namespace bookmarks
