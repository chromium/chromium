// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/resource_tracker.h"
#include "components/page_load_metrics/common/page_end_reason.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/scoped_visibility_tracker.h"

namespace content {
class WebContents;
}  // namespace content

namespace page_load_metrics {

namespace mojom {
class PageLoadMetadata;
}  // namespace mojom

struct UserInitiatedInfo;
struct PageRenderData;

// This class tracks global state for the page load that should be accessible
// from any PageLoadMetricsObserver.
class PageLoadMetricsObserverDelegate {
 public:
  virtual content::WebContents* GetWebContents() const = 0;

  // The time the navigation was initiated.
  virtual base::TimeTicks GetNavigationStart() const = 0;

  // The first time that the page was backgrounded since the navigation started.
  virtual const base::Optional<base::TimeDelta>& GetFirstBackgroundTime()
      const = 0;

  // The first time that the page was foregrounded since the navigation started.
  virtual const base::Optional<base::TimeDelta>& GetFirstForegroundTime()
      const = 0;

  // True if the page load started in the foreground.
  virtual bool StartedInForeground() const = 0;

  // Whether the page load was initiated by a user.
  virtual const UserInitiatedInfo& GetUserInitiatedInfo() const = 0;

  // Most recent URL for this page. Can be updated at navigation start, upon
  // redirection, and at commit time.
  virtual const GURL& GetUrl() const = 0;

  // The URL that started the navigation, before redirects.
  virtual const GURL& GetStartUrl() const = 0;

  // Whether the navigation for this page load committed.
  virtual bool DidCommit() const = 0;

  // The reason the page load ended. If the page is still active,
  // |page_end_reason| will be |END_NONE|. |page_end_time| contains the duration
  // of time until the cause of the page end reason was encountered.
  virtual PageEndReason GetPageEndReason() const = 0;

  // Whether the end reason for this page load was user initiated. For example,
  // if
  // this page load was ended due to a new navigation, this field tracks whether
  // that new navigation was user-initiated. This field is only useful if this
  // page load's end reason is a value other than END_NONE. Note that this
  // value is currently experimental, and is subject to change. In particular,
  // this field is not currently set for some end reasons, such as stop and
  // close, since we don't yet have sufficient instrumentation to know if a stop
  // or close was caused by a user action.
  //
  // TODO(csharrison): If more metadata for end reasons is needed we should
  // provide a better abstraction. Note that this is an approximation.
  virtual const UserInitiatedInfo& GetPageEndUserInitiatedInfo() const = 0;

  // Total lifetime of the page from the user's standpoint, starting at
  // navigation start. The page lifetime ends when the first of the following
  // events happen:
  // * the load of the main resource fails
  // * the page load is stopped
  // * the tab hosting the page is closed
  // * the render process hosting the page goes away
  // * a new navigation which later commits is initiated in the same tab
  // This field will not be set if the page is still active and hasn't yet
  // finished.
  virtual base::Optional<base::TimeDelta> GetPageEndTime() const = 0;

  // Extra information supplied to the page load metrics system from the
  // renderer for the main frame.
  virtual const mojom::PageLoadMetadata& GetMainFrameMetadata() const = 0;

  // PageLoadMetadata for subframes of the current page load.
  virtual const mojom::PageLoadMetadata& GetSubframeMetadata() const = 0;
  virtual const PageRenderData& GetPageRenderData() const = 0;
  virtual const PageRenderData& GetMainFrameRenderData() const = 0;
  virtual const ui::ScopedVisibilityTracker& GetVisibilityTracker() const = 0;
  virtual const ResourceTracker& GetResourceTracker() const = 0;

  // UKM SourceId for the current page load.
  virtual ukm::SourceId GetSourceId() const = 0;

  // Whether the associated navigation is the first navigation in its associated
  // WebContents. Note that, for newly opened tabs that display the New Tab
  // Page, the New Tab Page is considered the first navigation in that tab.
  virtual bool IsFirstNavigationInWebContents() const = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
