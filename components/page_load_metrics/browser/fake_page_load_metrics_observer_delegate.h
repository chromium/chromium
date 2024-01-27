// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_FAKE_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_FAKE_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/common/page_end_reason.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace page_load_metrics {

// A fake page load metrics observer delegate for use in unit tests.
// Almost all of the methods in this implementation simply return a backing
// instance variable (defaulting to null/zero values). Tests should configure
// the delegate's backing variables as appropriate for their scenarios. One
// exception is the BackForwardCacheRestore related methods - the
// BackForwardCacheRestore state structs are stored in an internal vector. Use
// |AddBackForwardCacheRestore| and |ClearBackForwardCacheRestores| to manage
// the fake delegate's vector of BackForwardCacheRestore states.
// If a backing instance variable doesn't exist for a method you need to fake,
// please add it with an appropriate default/zero value.
class FakePageLoadMetricsObserverDelegate
    : public PageLoadMetricsObserverDelegate {
 public:
  FakePageLoadMetricsObserverDelegate();
  virtual ~FakePageLoadMetricsObserverDelegate();

  // PageLoadMetricsObserverDelegate
  content::WebContents* GetWebContents() const override;
  base::TimeTicks GetNavigationStart() const override;
  std::optional<base::TimeDelta> GetTimeToFirstBackground() const override;
  std::optional<base::TimeDelta> GetTimeToFirstForeground() const override;
  PrerenderingState GetPrerenderingState() const override;
  std::optional<base::TimeDelta> GetActivationStart() const override;
  // By default no BackForwardCacheRestores are present, tests can add them by
  // calling |AddBackForwardCacheRestore|.
  const BackForwardCacheRestore& GetBackForwardCacheRestore(
      size_t index) const override;
  bool StartedInForeground() const override;
  PageVisibility GetVisibilityAtActivation() const override;
  bool WasPrerenderedThenActivatedInForeground() const override;
  const UserInitiatedInfo& GetUserInitiatedInfo() const override;
  const GURL& GetUrl() const override;
  const GURL& GetStartUrl() const override;
  bool DidCommit() const override;
  PageEndReason GetPageEndReason() const override;
  const UserInitiatedInfo& GetPageEndUserInitiatedInfo() const override;
  std::optional<base::TimeDelta> GetTimeToPageEnd() const override;
  const base::TimeTicks& GetPageEndTime() const override;
  const mojom::FrameMetadata& GetMainFrameMetadata() const override;
  const mojom::FrameMetadata& GetSubframeMetadata() const override;
  const PageRenderData& GetPageRenderData() const override;
  // Note: The argument to this method is ignored and normalized_cls_data_ is
  // returned.
  const NormalizedCLSData& GetNormalizedCLSData(
      BfcacheStrategy bfcache_strategy) const override;
  const NormalizedCLSData& GetSoftNavigationIntervalNormalizedCLSData()
      const override;
  const ResponsivenessMetricsNormalization&
  GetResponsivenessMetricsNormalization() const override;
  const ResponsivenessMetricsNormalization&
  GetSoftNavigationIntervalResponsivenessMetricsNormalization() const override;
  const mojom::InputTiming& GetPageInputTiming() const override;
  const std::optional<blink::SubresourceLoadMetrics>&
  GetSubresourceLoadMetrics() const override;
  const PageRenderData& GetMainFrameRenderData() const override;
  const ui::ScopedVisibilityTracker& GetVisibilityTracker() const override;
  const ResourceTracker& GetResourceTracker() const override;
  const LargestContentfulPaintHandler& GetLargestContentfulPaintHandler()
      const override;
  const LargestContentfulPaintHandler&
  GetExperimentalLargestContentfulPaintHandler() const override;
  ukm::SourceId GetPageUkmSourceId() const override;
  mojom::SoftNavigationMetrics& GetSoftNavigationMetrics() const override;
  ukm::SourceId GetUkmSourceIdForSoftNavigation() const override;
  ukm::SourceId GetPreviousUkmSourceIdForSoftNavigation() const override;
  bool IsFirstNavigationInWebContents() const override;
  bool IsOriginVisit() const override;
  bool IsTerminalVisit() const override;
  int64_t GetNavigationId() const override;

  // Helpers to add a BackForwardCacheRestore to this fake.
  void AddBackForwardCacheRestore(BackForwardCacheRestore bfcache_restore);
  // Clears all the store BackForwardCacheRestores.
  void ClearBackForwardCacheRestores();

  // These instance variables will be returned by calls to the method with the
  // corresponding name. Tests should set these variables appropriately.
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  UserInitiatedInfo user_initiated_info_;
  GURL url_;
  GURL start_url_;
  PageEndReason page_end_reason_ = page_load_metrics::END_NONE;
  UserInitiatedInfo page_end_user_initiated_info_;
  base::TimeTicks page_end_time_;
  mojom::FrameMetadata main_frame_metadata_;
  mojom::FrameMetadata subframe_metadata_;
  PageRenderData page_render_data_;
  NormalizedCLSData normalized_cls_data_;
  ResponsivenessMetricsNormalization responsiveness_metrics_normalization_;
  mojom::InputTiming page_input_timing_;
  std::optional<blink::SubresourceLoadMetrics> subresource_load_metrics_;
  PageRenderData main_frame_render_data_;
  ui::ScopedVisibilityTracker visibility_tracker_;
  ResourceTracker resource_tracker_;
  LargestContentfulPaintHandler largest_contentful_paint_handler_;
  LargestContentfulPaintHandler experimental_largest_contentful_paint_handler_;
  int64_t navigation_id_;
  base::TimeTicks navigation_start_;
  std::optional<base::TimeTicks> first_background_time_ = std::nullopt;
  bool started_in_foreground_ = true;
  PrerenderingState prerendering_state_ = PrerenderingState::kNoPrerendering;
  PageVisibility visibility_at_activation_ = PageVisibility::kNotInitialized;
  std::optional<base::TimeDelta> activation_start_ = std::nullopt;

  // This vector backs the |GetBackForwardCacheRestore| and
  // |GetMostRecentBackForwardCacheRestore| methods.
  std::vector<BackForwardCacheRestore> back_forward_cache_restores_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_FAKE_PAGE_LOAD_METRICS_OBSERVER_DELEGATE_H_
