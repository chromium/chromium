// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/web_app_offline.h"

#include <stdint.h>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/browser/web_app_error_page_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/web_app_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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
  DefaultOfflineWebContentsObserver(content::WebContents* web_contents,
                                    int64_t error_page_navigation_id)
      : WebContentsObserver(web_contents),
        error_page_navigation_id_(error_page_navigation_id) {}

  DefaultOfflineWebContentsObserver() = delete;

 private:
  // WebContentsObserver:

  void WebContentsDestroyed() override {
    CHECK(!error_page_url_.is_empty());
    LogAndExit(ClosingReason::kWebContentsDestroyed);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    // If we have no `error_page_url_`, wait for the tracked error-page
    // navigation to finish. Since this class is created before the error page
    // commits in the renderer, the navigation may be aborted or discarded
    // during WebContents shutdown without committing.
    if (error_page_url_.is_empty()) {
      if (navigation_handle->GetNavigationId() != error_page_navigation_id_) {
        return;
      }
      if (!navigation_handle->HasCommitted()) {
        delete this;
        return;
      }
      CHECK(navigation_handle->IsErrorPage());
      error_page_url_ = navigation_handle->GetURL();
      timer_start_ = base::TimeTicks::Now();
      return;
    }

    if (!navigation_handle->HasCommitted()) {
      return;
    }

    if (error_page_url_ != navigation_handle->GetURL()) {
      // Note: After calling this function, the class deletes itself.
      LogAndExit(ClosingReason::kNewNavigation);
    } else {
      // Note: After calling this function, the class deletes itself.
      LogAndExit(ClosingReason::kNetworkReestablished);
    }
  }

  // Logs the metrics for the error page and deletes self. This class should not
  // be used after calling this function.
  void LogAndExit(ClosingReason reason) {
    CHECK(!timer_start_.is_null());
    base::TimeDelta delta = base::TimeTicks::Now() - timer_start_;
    UMA_HISTOGRAM_ENUMERATION("WebApp.DefaultOffline.ClosingReason", reason);
    UMA_HISTOGRAM_CUSTOM_TIMES("WebApp.DefaultOffline.DurationShown", delta,
                               base::Seconds(1), base::Hours(1),
                               /* bucket_count= */ 100);

    delete this;  // No further processing should take place after this point.
  }

  const int64_t error_page_navigation_id_;
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

void TrackOfflinePageVisibility(content::NavigationHandle& navigation_handle) {
  CHECK(navigation_handle.IsInPrimaryMainFrame());
  CHECK(!navigation_handle.HasCommitted());
  content::WebContents* web_contents = navigation_handle.GetWebContents();
  if (web_contents == nullptr) {
    return;
  }

  // This class manages its own lifetime.
  new DefaultOfflineWebContentsObserver(web_contents,
                                        navigation_handle.GetNavigationId());
}

}  // namespace web_app
