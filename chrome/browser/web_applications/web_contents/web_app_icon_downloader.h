// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_ICON_DOWNLOADER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_ICON_DOWNLOADER_H_

#include <map>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

class SkBitmap;

namespace gfx {
class Size;
}

namespace web_app {

enum class IconsDownloadedResult;

struct IconDownloaderOptions {
  // If favicons from the current WebContents should not be downloaded.
  bool skip_page_favicons = false;
  // If the download should end early if any failures occur.
  bool fail_all_if_any_fail = false;
};

// Class to help download all icons (including favicons and web app manifest
// icons) for a tab.
class WebAppIconDownloader : public content::WebContentsObserver {
 public:
  using WebAppIconDownloaderCallback =
      base::OnceCallback<void(IconsDownloadedResult result,
                              IconsMap icons_map,
                              DownloadedIconsHttpResults icons_http_results)>;

  // Exposed for testing.
  static constexpr base::TimeDelta kDefaultSecondsToWaitForIconDownloading =
      base::Seconds(30);

  WebAppIconDownloader();
  WebAppIconDownloader(const WebAppIconDownloader&) = delete;
  WebAppIconDownloader& operator=(const WebAppIconDownloader&) = delete;
  ~WebAppIconDownloader() override;

  // |extra_icon_urls_with_sizes| allows callers to provide icon urls that
  // aren't provided by the renderer (e.g touch icons on non-android
  // environments).
  virtual void Start(content::WebContents* web_contents,
                     const IconUrlSizeSet& extra_icon_urls_with_sizes,
                     WebAppIconDownloaderCallback callback,
                     IconDownloaderOptions options = IconDownloaderOptions());

  size_t pending_requests() const { return in_progress_requests_.size(); }

 private:
  // This is used for metrics, do not change values.
  enum class WebAppIconDownloaderResult {
    kSuccess = 0,
    kPrimaryPageChanged = 1,
    kFailure = 2,
    kTimeoutFailure = 3,
    kTimeoutWithPartialDownloadSuccess = 4,
    kMaxValue = kTimeoutWithPartialDownloadSuccess
  };

  // Returns if this downloader is still 'running', where the callback hasn't
  // been called yet.
  bool IsRunning();

  // Initiates a download of the image at |url| and returns the download id.
  int DownloadImage(const GURL& url, const gfx::Size& preferred_size);

  // Queries WebContents for the page's current favicon URLs.
  const std::vector<blink::mojom::FaviconURLPtr>&
  GetFaviconURLsFromWebContents();

  // Fetches icons for the given urls with sizes.
  // |callback_| is run when all downloads complete.
  void FetchIcons(const IconUrlSizeSet& urls_to_download_with_size);

  // Icon download callback.
  void DidDownloadFavicon(int id,
                          int http_status_code,
                          const GURL& image_url,
                          const std::vector<SkBitmap>& bitmaps,
                          const std::vector<gfx::Size>& original_bitmap_sizes);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

  void MaybeCompleteCallback();
  void CancelDownloads(IconsDownloadedResult result,
                       DownloadedIconsHttpResults icons_http_results,
                       WebAppIconDownloaderResult metrics_result);

  // Used to abort icon downloading if the timer_ has elapsed 30 seconds
  // (by default) and either exit with a failure based on:
  // 1. if no icons have been downloaded.
  // 2. fail_all_if_fail_any has been set as true.
  // For other cases, it returns the icons that have been successfully
  // downloaded so far, and reports HTTP_REQUEST_TIMEOUT for the rest.
  void OnTimeout();

  IconDownloaderOptions options_;

  // The icons which were downloaded. Populated by FetchIcons().
  IconsMap icons_map_;
  // The http status codes resulted from url downloading requests.
  DownloadedIconsHttpResults icons_http_results_;

  // Request ids of in-progress requests.
  bool populating_pending_requests_ = false;
  std::map<int, IconUrlWithSize> in_progress_requests_;

  // Urls for which a download has already been initiated. Used to prevent
  // duplicate downloads of the same url.
  IconUrlSizeSet processed_urls_;

  // Timer used for the timeout result.
  base::OneShotTimer timer_;

  // Callback to run on favicon download completion.
  WebAppIconDownloaderCallback callback_;

  base::WeakPtrFactory<WebAppIconDownloader> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_CONTENTS_WEB_APP_ICON_DOWNLOADER_H_
