// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_DOWNLOADER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_DOWNLOADER_H_

#include <map>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

class SkBitmap;

namespace gfx {
class Size;
}

namespace web_app {

// Class to help download all icons (including favicons and web app manifest
// icons) for a tab.
class WebAppIconDownloader : public content::WebContentsObserver {
 public:
  enum class Histogram {
    kForCreate,
    kForSync,
    kForUpdate,
  };

  using WebAppIconDownloaderCallback =
      base::OnceCallback<void(bool success, IconsMap icons_map)>;

  // |extra_favicon_urls| allows callers to provide icon urls that aren't
  // provided by the renderer (e.g touch icons on non-android environments).
  // |https_status_code_class_histogram_name| optionally specifies a histogram
  // to use for logging http status code class results from fetch attempts.
  WebAppIconDownloader(content::WebContents* web_contents,
                       const std::vector<GURL>& extra_favicon_urls,
                       Histogram histogram,
                       WebAppIconDownloaderCallback callback);
  WebAppIconDownloader(const WebAppIconDownloader&) = delete;
  WebAppIconDownloader& operator=(const WebAppIconDownloader&) = delete;
  ~WebAppIconDownloader() override;

  // Instructs the downloader to not query the page for favicons (e.g. when a
  // favicon URL has already been provided in the constructor's
  // |extra_favicon_urls| argument).
  void SkipPageFavicons();

  void FailAllIfAnyFail();

  void Start();

 private:
  friend class TestWebAppIconDownloader;

  // Initiates a download of the image at |url| and returns the download id.
  // This is overridden in testing.
  virtual int DownloadImage(const GURL& url);

  // Queries WebContents for the page's current favicon URLs.
  // This is overridden in testing.
  virtual const std::vector<blink::mojom::FaviconURLPtr>&
  GetFaviconURLsFromWebContents();

  // Fetches icons for the given urls.
  // |callback_| is run when all downloads complete.
  void FetchIcons(const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls);
  void FetchIcons(const std::vector<GURL>& urls);

  // Icon download callback.
  void DidDownloadFavicon(int id,
                          int http_status_code,
                          const GURL& image_url,
                          const std::vector<SkBitmap>& bitmaps,
                          const std::vector<gfx::Size>& original_bitmap_sizes);

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* rfh,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;

  void CancelDownloads();

  // Whether we need to fetch favicons from the renderer.
  bool need_favicon_urls_;

  // Whether we consider all requests to have failed if any individual URL fails
  // to load.
  bool fail_all_if_any_fail_;

  // URLs that aren't given by WebContentsObserver::DidUpdateFaviconURL() that
  // should be used for this favicon. This is necessary in order to get touch
  // icons on non-android environments.
  std::vector<GURL> extra_favicon_urls_;

  // The icons which were downloaded. Populated by FetchIcons().
  IconsMap icons_map_;

  // Request ids of in-progress requests.
  std::set<int> in_progress_requests_;

  // Urls for which a download has already been initiated. Used to prevent
  // duplicate downloads of the same url.
  std::set<GURL> processed_urls_;

  // Callback to run on favicon download completion.
  WebAppIconDownloaderCallback callback_;

  // Which histogram to log individual fetch results under.
  Histogram histogram_;

  base::WeakPtrFactory<WebAppIconDownloader> weak_ptr_factory_{this};

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_ICON_DOWNLOADER_H_
