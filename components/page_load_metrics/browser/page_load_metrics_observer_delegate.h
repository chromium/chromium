// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_

#include <optional>

#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/resource_tracker.h"
#include "components/page_load_metrics/browser/responsiveness_metrics_normalization.h"
#include "components/page_load_metrics/common/page_end_reason.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/scoped_visibility_tracker.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace page_load_metrics {

namespace mojom {
class FrameMetadata;
}  // namespace mojom

struct UserInitiatedInfo;
struct PageRenderData;
struct NormalizedCLSData;

// Represents the page's visibility at a specific timing.
enum class PageVisibility {
  kNotInitialized = 0,
  kForeground = 1,
  kBackground = 2,
  kMaxValue = kBackground,
};

// Represents the page's state of prerendering.
// If the page is previewed, the state starts with kInPreview, and may be
// transitted to kNoPrerendering after its activation and promotion.
// If the page is prerendereed, the state starts with kInPrerendering, and may
// be transmitted to kActivatedNoActivationStart, and kActivated.
// Otherwise, it sticks on kNoPrerendering.
//
// TODO(crbug.com/40233224): Remove kActivatedNoActivationStart if possible.
enum class PrerenderingState {
  // Not prerenedered
  kNoPrerendering,
  // Previewed before acitvation and promotion
  kInPreview,
  // Prerendered before activation
  kInPrerendering,
  // Prerendered and activated, but `PageLoadTiming.activation_start` is not
  // arrived
  //
  // In many cases, PageLoadMetricsObservers can regard this state
  // kInPrerendering.
  kActivatedNoActivationStart,
  // Prerendered and activated
  kActivated,
};

// This class tracks global state for the page load that should be accessible
// from any PageLoadMetricsObserver.
class PageLoadMetricsObserverDelegate {
 public:
  // States when the page is restored from the back-forward cache.
  struct BackForwardCacheRestore {
    explicit BackForwardCacheRestore(bool was_in_foreground,
                                     base::TimeTicks navigation_start_time);
    BackForwardCacheRestore(const BackForwardCacheRestore&);

    // The first time when the page becomes backgrounded after the page is
    // restored. The time is relative to the navigation start of bfcache restore
    // navigation.
    std::optional<base::TimeDelta> first_background_time;

    // The navigation start time for this back-forward cache restore.
    base::TimeTicks navigation_start_time;

    // True if the page was in foreground when the page is restored.
    bool was_in_foreground = false;
  };

  // Distinguishes metric variants by how they react to bfcache events.
  enum class BfcacheStrategy {
    // Accumulate the metric over the lifetime of the PageLoadTracker.
    ACCUMULATE,
    // Reset the metric when the page enters the bfcache.
    RESET
  };

  virtual content::WebContents* GetWebContents() const = 0;

  // The time the navigation was initiated.
  virtual base::TimeTicks GetNavigationStart() const = 0;

  // The id of the main navigation associated with this page, which created the
  // main document. Not updated for same-document navigations.
  virtual int64_t GetNavigationId() const = 0;

  // The duration until the first time that the page was backgrounded since the
  // navigation started. Will be nullopt if the page has never been
  // backgrounded.
  virtual std::optional<base::TimeDelta> GetTimeToFirstBackground() const = 0;

  // The duration until the first time that the page was foregrounded since the
  // navigation started. Will be nullopt if the page has never been in the
  // foreground.
  virtual std::optional<base::TimeDelta> GetTimeToFirstForeground() const = 0;

  // The state of index-th restore from the back-forward cache.
  virtual const BackForwardCacheRestore& GetBackForwardCacheRestore(
      size_t index) const = 0;

  // True if the page load started in the foreground.
  virtual bool StartedInForeground() const = 0;
  // Page's visibility at activation.
  virtual PageVisibility GetVisibilityAtActivation() const = 0;

  // True if the page load was a prerender, that was later activated by a
  // navigation that started in the foreground.
  virtual bool WasPrerenderedThenActivatedInForeground() const = 0;
  // The prerendering state.
  virtual PrerenderingState GetPrerenderingState() const = 0;
  // True iff the page is prerendered and activation_start is not yet arrived.
  bool IsInPrerenderingBeforeActivationStart() const;
  // Returns activation start if activation start was arrived, or nullopt.
  virtual std::optional<base::TimeDelta> GetActivationStart() const = 0;

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
  virtual std::optional<base::TimeDelta> GetTimeToPageEnd() const = 0;

  // The absolute time at which the page's lifetime ended. See the comment
  // on GetTimeToPageEnd for the definition of when a page's lifetime ends.
  virtual const base::TimeTicks& GetPageEndTime() const = 0;

  // Extra information supplied to the page load metrics system from the
  // renderer for the main frame.
  virtual const mojom::FrameMetadata& GetMainFrameMetadata() const = 0;

  // FrameMetadata for subframes of the current page load. This contains
  // aggregate information across all subframes. Non-aggregatable information
  // such as subframe intersections is initialized to defaults.
  virtual const mojom::FrameMetadata& GetSubframeMetadata() const = 0;
  virtual const PageRenderData& GetPageRenderData() const = 0;
  virtual const NormalizedCLSData& GetNormalizedCLSData(
      BfcacheStrategy bfcache_strategy) const = 0;
  virtual const NormalizedCLSData& GetSoftNavigationIntervalNormalizedCLSData()
      const = 0;
  // Returns normalized responsiveness metrics data. Normalization explained in
  // https://web.dev/inp.
  virtual const ResponsivenessMetricsNormalization&
  GetResponsivenessMetricsNormalization() const = 0;

  virtual const ResponsivenessMetricsNormalization&
  GetSoftNavigationIntervalResponsivenessMetricsNormalization() const = 0;

  // InputTiming data accumulated across all frames.
  virtual const mojom::InputTiming& GetPageInputTiming() const = 0;
  virtual const PageRenderData& GetMainFrameRenderData() const = 0;
  virtual const ui::ScopedVisibilityTracker& GetVisibilityTracker() const = 0;
  virtual const ResourceTracker& GetResourceTracker() const = 0;
  virtual const std::optional<blink::SubresourceLoadMetrics>&
  GetSubresourceLoadMetrics() const = 0;

  // Returns a shared LargestContentfulPaintHandler for page load metrics.
  virtual const LargestContentfulPaintHandler&
  GetLargestContentfulPaintHandler() const = 0;
  // Returns a LargestContentfulPaintHandler for the experimental version of
  // LCP. Note that currently this 'experimental version' is the version that is
  // being deprecated.
  virtual const LargestContentfulPaintHandler&
  GetExperimentalLargestContentfulPaintHandler() const = 0;

  // Returns the current soft navigation count - https://bit.ly/soft-navigation
  // Soft navigations are JS-driven same-document navigations that are using the
  // history API or the new Navigation API, triggered by a user gesture and
  // meaningfully modify the DOM, replacing the previous content with new one.
  virtual mojom::SoftNavigationMetrics& GetSoftNavigationMetrics() const = 0;

  // UKM source ID for the current soft navigation.
  virtual ukm::SourceId GetUkmSourceIdForSoftNavigation() const = 0;

  // UKM source ID for the previous soft navigation.
  virtual ukm::SourceId GetPreviousUkmSourceIdForSoftNavigation() const = 0;

  // UKM source ID for the current page load.
  // Note: For prerendered page loads, this returns ukm::kInvalidSourceId until
  // the activation navigation. After activation, this returns a UKM source ID
  // associated with the activation navigation's ID.
  virtual ukm::SourceId GetPageUkmSourceId() const = 0;

  // Whether the associated navigation is the first navigation in its associated
  // WebContents. Note that, for newly opened tabs that display the New Tab
  // Page, the New Tab Page is considered the first navigation in that tab.
  virtual bool IsFirstNavigationInWebContents() const = 0;

  // Checks whether the associated page visit is the first visit in its
  // associated WebContesnts, or navigated from a Chrome UI, such as Omnibox or
  // Bookmarks.
  // As we don't identify client redirect cases, if the origin page runs client
  // redirects, only the redirect initiating page is marked as the origin visit,
  // and actual landing page is not marked as the origin visit.
  virtual bool IsOriginVisit() const = 0;

  // Checks whether the associated page visit doesn't see any link navigation.
  // If the next navigation is initiated from a Chrome UI, the current page will
  // be marked as a terminal visit unless it made another link navigation and
  // went back to the page with a back navigation from BFCache.
  virtual bool IsTerminalVisit() const = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
