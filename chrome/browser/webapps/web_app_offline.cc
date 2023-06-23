// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/web_app_offline.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// These values are persistent to logs. Entries should not be renumbered and
// numeric values should never be reused. This should match the enum
// ClosingReason in tools/metrics/histograms/enums.xml.
enum class ClosingReason {
  kNetworkReestablished = 0,
  kNewNavigation = 1,
  kWebContentsDestroyed = 2,
  kMaxValue = kWebContentsDestroyed,
};

// This class keeps track of how long the DefaultOffline page is shown, before
// either a navigation happens (e.g. when the connection is re-established), or
// the web_contents dies (e.g. because the user killed the app). The class
// manages its own lifetime by deleting itself when either of those two
// conditions above are met.
class DefaultOfflineWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit DefaultOfflineWebContentsObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  DefaultOfflineWebContentsObserver() = delete;

 private:
  // WebContentsObserver:

  void WebContentsDestroyed() override {
    // Note: After calling this function, the class deletes itself.
    LogAndExit(ClosingReason::kWebContentsDestroyed);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsSameDocument())
      return;

    // The first successful navigation ending is always the error page, where we
    // set the `error_page_url_`. A new navigation (after that) means we should
    // end tracking, because either the network connection is back online (and
    // the app has successfully loaded), or the user has navigated away from the
    // app.
    if (!error_page_url_.is_empty()) {
      if (error_page_url_ != navigation_handle->GetURL()) {
        // Note: After calling this function, the class deletes itself.
        LogAndExit(ClosingReason::kNewNavigation);
      } else {
        // Note: After calling this function, the class deletes itself.
        LogAndExit(ClosingReason::kNetworkReestablished);
      }
      return;
    }

    // If we have no error_page_url_ then this is the end of the first
    // navigation. Since this class is created when we detect an error page is
    // about to be shown, we just need to grab the URL and start the timer.
    DCHECK(navigation_handle->IsErrorPage());
    DCHECK(error_page_url_.is_empty());
    error_page_url_ = navigation_handle->GetURL();
    timer_start_ = base::TimeTicks::Now();
  }

  // Logs the metrics for the error page and deletes self. This class should not
  // be used after calling this function.
  void LogAndExit(ClosingReason reason) {
    base::TimeDelta delta = base::TimeTicks::Now() - timer_start_;
    UMA_HISTOGRAM_ENUMERATION("WebApp.DefaultOffline.ClosingReason", reason);
    UMA_HISTOGRAM_CUSTOM_TIMES("WebApp.DefaultOffline.DurationShown", delta,
                               base::Seconds(1), base::Hours(1),
                               /* bucket_count= */ 100);

    delete this;  // No further processing should take place after this point.
  }

  GURL error_page_url_;
  base::TimeTicks timer_start_;
};

}  // namespace

namespace web_app {

#if !BUILDFLAG(IS_ANDROID)
content::mojom::AlternativeErrorPageOverrideInfoPtr GetOfflinePageInfo(
    const GURL& url,
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context) {
  return ConstructWebAppErrorPage(
      url, render_frame_host, browser_context,
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_HEADING_YOU_ARE_OFFLINE),
      error_page::kOfflineIconId);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void TrackOfflinePageVisibility(content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == nullptr)
    return;  // Some browser_tests pass a null `render_frame_host`.

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // This class manages its own lifetime.
  new DefaultOfflineWebContentsObserver(web_contents);
}

}  // namespace web_app
