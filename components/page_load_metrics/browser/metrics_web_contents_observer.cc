// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/page_load_metrics/browser/metrics_lifecycle_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"
#include "components/page_load_metrics/browser/page_load_metrics_update_dispatcher.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/base/page_transition_types.h"

namespace page_load_metrics {

namespace {

// Returns the HTTP status code for the current page, or -1 if no status code
// is available. Can only be called if the `navigation_handle` has committed.
int GetHttpStatusCode(content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle->HasCommitted());
  const net::HttpResponseHeaders* response_headers =
      navigation_handle->GetResponseHeaders();
  if (!response_headers) {
    return -1;
  }
  return response_headers->response_code();
}

UserInitiatedInfo CreateUserInitiatedInfo(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsRendererInitiated()) {
    return UserInitiatedInfo::BrowserInitiated();
  }

  return UserInitiatedInfo::RenderInitiated(
      navigation_handle->HasUserGesture(),
      !navigation_handle->NavigationInputStart().is_null());
}

}  // namespace

// static
void MetricsWebContentsObserver::RecordFeatureUsage(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::WebFeature>& web_features) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  MetricsWebContentsObserver* observer =
      MetricsWebContentsObserver::FromWebContents(web_contents);

  if (observer) {
    std::vector<blink::UseCounterFeature> features;
    for (auto web_feature : web_features) {
      CHECK_NE(web_feature, blink::mojom::WebFeature::kPageVisits)
          << "WebFeature::kPageVisits is a reserved feature.";
      if (web_feature == blink::mojom::WebFeature::kPageVisits) {
        continue;
      }

      features.emplace_back(blink::mojom::UseCounterFeatureType::kWebFeature,
                            static_cast<uint32_t>(web_feature));
    }
    observer->OnBrowserFeatureUsage(render_frame_host, features);
  }
}

// static
void MetricsWebContentsObserver::RecordFeatureUsage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, std::vector<blink::mojom::WebFeature>{feature});
}

// static
void MetricsWebContentsObserver::RecordFeatureUsage(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::WebDXFeature>& webdx_features) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  MetricsWebContentsObserver* observer =
      MetricsWebContentsObserver::FromWebContents(web_contents);

  if (observer) {
    std::vector<blink::UseCounterFeature> features;
    for (auto webdx_feature : webdx_features) {
      CHECK_NE(webdx_feature, blink::mojom::WebDXFeature::kPageVisits)
          << "WebFeature::kPageVisits is a reserved feature.";
      if (webdx_feature == blink::mojom::WebDXFeature::kPageVisits) {
        continue;
      }

      features.emplace_back(blink::mojom::UseCounterFeatureType::kWebDXFeature,
                            static_cast<uint32_t>(webdx_feature));
    }
    observer->OnBrowserFeatureUsage(render_frame_host, features);
  }
}

// static
void MetricsWebContentsObserver::RecordFeatureUsage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebDXFeature feature) {
  MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, std::vector<blink::mojom::WebDXFeature>{feature});
}

// static
MetricsWebContentsObserver* MetricsWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface) {
  CHECK(web_contents);

  MetricsWebContentsObserver* metrics = FromWebContents(web_contents);
  if (!metrics) {
    metrics = new MetricsWebContentsObserver(web_contents,
                                             std::move(embedder_interface));
    web_contents->SetUserData(UserDataKey(), base::WrapUnique(metrics));
    metrics->created_ = base::TimeTicks::Now();
  }
  return metrics;
}

// static
void MetricsWebContentsObserver::BindPageLoadMetrics(
    mojo::PendingAssociatedReceiver<mojom::PageLoadMetrics> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }
  auto* observer = MetricsWebContentsObserver::FromWebContents(web_contents);
  if (!observer) {
    return;
  }
  observer->page_load_metrics_receivers_.Bind(rfh, std::move(receiver));
}

MetricsWebContentsObserver::~MetricsWebContentsObserver() = default;

void MetricsWebContentsObserver::WebContentsWillSoonBeDestroyed() {
  // TODO(crbug.com/40238907): Should not rely on this call.
  // This method is called only in a certain situation, and most embedders
  // doesn't support to call this method before WebContentsDestroyed().
  web_contents_will_soon_be_destroyed_ = true;
}

void MetricsWebContentsObserver::WebContentsDestroyed() {
  // TODO(csharrison): Use a more user-initiated signal for CLOSE.
  NotifyPageEndAllLoads(END_CLOSE, UserInitiatedInfo::NotUserInitiated());

  // Do this before clearing `primary_page_`, so that the observers don't hit
  // the CHECK in MetricsWebContentsObserver::GetDelegateForCommittedLoad.
  for (auto& observer : lifecycle_observers_) {
    observer.OnGoingAway();
  }

  UnregisterInputEventObserver(web_contents()->GetPrimaryMainFrame());

  // We tear down PageLoadTrackers in WebContentsDestroyed, rather than in the
  // destructor, since `web_contents()` returns nullptr in the destructor, and
  // PageLoadMetricsObservers can cause code to execute that wants to be able to
  // access the current WebContents.
  primary_page_ = nullptr;
  active_pages_.clear();
  ukm_smoothness_data_.clear();
  provisional_loads_.clear();
  aborted_provisional_loads_.clear();
}

void MetricsWebContentsObserver::RegisterInputEventObserver(
    content::RenderFrameHost* host) {
  if (host != nullptr) {
    host->GetRenderWidgetHost()->AddInputEventObserver(this);
  }
}

void MetricsWebContentsObserver::UnregisterInputEventObserver(
    content::RenderFrameHost* host) {
  if (host != nullptr) {
    host->GetRenderWidgetHost()->RemoveInputEventObserver(this);
  }
}

void MetricsWebContentsObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (!new_host->IsInPrimaryMainFrame()) {
    return;
  }

  UnregisterInputEventObserver(old_host);
  RegisterInputEventObserver(new_host);
}

void MetricsWebContentsObserver::FrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  content::RenderFrameHost* rfh =
      web_contents()->UnsafeFindFrameByFrameTreeNodeId(frame_tree_node_id);
  if (!rfh) {
    return;
  }

  // Deletion of FrameTreeNode follows deletion of RenderFrameHost. If the node
  // is root of the page, corresponding PageLoadTracker has gone at this timing.
  // So, PageLoadTracker cannot forward a deletion event of FrameTreeNode for
  // itself and MetrcisWebContents does this role.
  if (PageLoadTracker* tracker = GetAncestralAlivePageLoadTracker(rfh)) {
    tracker->FrameTreeNodeDeleted(frame_tree_node_id);
  }
}

void MetricsWebContentsObserver::RenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  if (auto* memory_tracker = GetMemoryTracker()) {
    memory_tracker->OnRenderFrameDeleted(rfh, this);
  }

  if (PageLoadTracker* tracker = GetPageLoadTracker(rfh)) {
    tracker->RenderFrameDeleted(rfh);
  }

  content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
  auto new_end_it = std::remove_if(queued_memory_updates_.begin(),
                                   queued_memory_updates_.end(),
                                   [rfh_id](const MemoryUpdate& update) {
                                     return update.routing_id == rfh_id;
                                   });
  queued_memory_updates_.erase(new_end_it, queued_memory_updates_.end());

  // PageLoadTracker and smoothness data can be associated only with a main
  // frame.
  if (rfh->GetParent()) {
    return;
  }
  active_pages_.erase(rfh);
  inactive_pages_.erase(rfh);
  ukm_smoothness_data_.erase(rfh);
}

void MetricsWebContentsObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id) {
  auto* render_frame_host =
      content::RenderFrameHost::FromID(id.frame_routing_id);

  // Ignore media that starts playing in a page that was navigated away
  // from.
  if (PageLoadTracker* tracker = GetPageLoadTracker(render_frame_host)) {
    tracker->MediaStartedPlaying(video_type, render_frame_host);
  }
}

void MetricsWebContentsObserver::WillStartNavigationRequest(
    content::NavigationHandle* navigation_handle) {
  // Same-document navigations should never go through
  // WillStartNavigationRequest.
  CHECK(!navigation_handle->IsSameDocument());

  if (!navigation_handle->IsInMainFrame()) {
    return;
  }

  WillStartNavigationRequestImpl(navigation_handle);
  has_navigated_ = true;
}

MetricsWebContentsObserver::MetricsWebContentsObserver(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<MetricsWebContentsObserver>(*web_contents),
      in_foreground_(web_contents->GetVisibility() !=
                     content::Visibility::HIDDEN),
      embedder_interface_(std::move(embedder_interface)),
      has_navigated_(false),
      page_load_metrics_receivers_(web_contents, this) {
  // NoStatePrefetch loads erroneously report that they are initially visible,
  // so we manually override visibility state for prerender.
  if (embedder_interface_->IsNoStatePrefetch(web_contents)) {
    in_foreground_ = false;
  }

  RegisterInputEventObserver(web_contents->GetPrimaryMainFrame());
}

void MetricsWebContentsObserver::WillStartNavigationRequestImpl(
    content::NavigationHandle* navigation_handle) {
  UserInitiatedInfo user_initiated_info(
      CreateUserInitiatedInfo(navigation_handle));
  std::unique_ptr<PageLoadTracker> last_aborted =
      NotifyAbortedProvisionalLoadsNewNavigation(navigation_handle,
                                                 user_initiated_info);

  if (!ShouldTrackMainFrameNavigation(navigation_handle)) {
    return;
  }

  // Pass in the last committed url to the PageLoadTracker. If the MWCO has
  // never observed a committed load, use the last committed url from this
  // WebContent's opener. This is more accurate than using referrers due to
  // referrer sanitizing and origin referrers. Note that this could potentially
  // be inaccurate if the opener has since navigated.
  content::RenderFrameHost* opener = web_contents()->GetOpener();
  const GURL& opener_url =
      !has_navigated_ && opener ? opener->GetLastCommittedURL() : GURL();
  const GURL& currently_committed_url =
      primary_page_ ? primary_page_->url() : opener_url;

  bool in_foreground =
      !navigation_handle->IsInPrerenderedMainFrame() && in_foreground_;

  // Prepare ukm::SourceId that is based on outermost page's navigation ID.
  ukm::SourceId source_id = ukm::kInvalidSourceId;
  base::WeakPtr<PageLoadTracker> parent_tracker;
  if (navigation_handle->IsInPrimaryMainFrame()) {
    // Primary pages use own page's navigation ID.
    source_id = ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                                       ukm::SourceIdType::NAVIGATION_ID);
  } else if (navigation_handle->IsInPrerenderedMainFrame()) {
    // Prerendering pages should not record UKM until its activation. So, we
    // start with ukm::kInvalidSourceId and set a correct ukm::SourceId on
    // activation.
    CHECK_EQ(ukm::kInvalidSourceId, source_id);
  } else if (navigation_handle->GetNavigatingFrameType() ==
             content::FrameType::kFencedFrameRoot) {
    // For FencedFrames, use the primary page's ukm::SourceId. `primary_page_`
    // can be nullptr if the main frame is in data URL or so.
    if (primary_page_) {
      source_id = primary_page_->GetPageUkmSourceId();
      parent_tracker = primary_page_->GetWeakPtr();
    } else {
      // Use ukm::NoURLSourceId() rather than kInvalidSourceId to avoid
      // unexpected check failure. This happens on tests that create a
      // FencedFrame via FencedFrameTestHelper directly without a correct setup
      // being finished on the embedder frame.
      source_id = ukm::NoURLSourceId();
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  // For prerendered page activations, we don't create a new PageLoadTracker,
  // but reuse an existing one that was created for the initial prerendering
  // navigation so that the same instance will bee OnPrerenderStart and
  // DidActivatePrerenderedPage.
  if (navigation_handle->IsPrerenderedPageActivation()) {
    return;
  }

  // Passing raw pointers to `embedder_interface_` is safe because the
  // MetricsWebContentsObserver owns them both list and they are torn down after
  // the PageLoadTracker. The PageLoadTracker does not hold on to
  // `navigation_handle` beyond the scope of the constructor.
  auto insertion_result = provisional_loads_.insert(std::make_pair(
      navigation_handle,
      std::make_unique<PageLoadTracker>(
          in_foreground, embedder_interface_.get(), currently_committed_url,
          !has_navigated_, navigation_handle, user_initiated_info, source_id,
          parent_tracker)));
  CHECK(insertion_result.second)
      << "provisional_loads_ already contains NavigationHandle.";
  for (auto& observer : lifecycle_observers_) {
    observer.OnTrackerCreated(insertion_result.first->second.get());
  }
}

void MetricsWebContentsObserver::WillProcessNavigationResponse(
    content::NavigationHandle* navigation_handle) {
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end()) {
    return;
  }
  it->second->WillProcessNavigationResponse(navigation_handle);
}

PageLoadTracker* MetricsWebContentsObserver::GetTrackerOrNullForRequest(
    const content::GlobalRequestID& request_id,
    content::RenderFrameHost* render_frame_host_or_null,
    network::mojom::RequestDestination request_destination,
    base::TimeTicks creation_time) {
  if (request_destination == network::mojom::RequestDestination::kDocument) {
    CHECK(request_id != content::GlobalRequestID());
    // The main frame request can complete either before or after commit, so we
    // look at both provisional loads and the committed load to find a
    // PageLoadTracker with a matching request id. See https://goo.gl/6TzCYN for
    // more details.
    for (const auto& kv : provisional_loads_) {
      PageLoadTracker* candidate = kv.second.get();
      if (candidate->HasMatchingNavigationRequestID(request_id)) {
        return candidate;
      }
    }
    if (primary_page_ &&
        primary_page_->HasMatchingNavigationRequestID(request_id)) {
      return primary_page_.get();
    }
    if (auto page_pair = inactive_pages_.find(render_frame_host_or_null);
        page_pair != inactive_pages_.end()) {
      return page_pair->second.get();
    }
  } else {
    // Non main resources are always associated with the currently committed
    // load, `primary_page_` or `active_pages_`. If the resource
    // request was started before this navigation of them, then it should be
    // ignored. Check `primary_page_` here as its start time is the oldest one.
    if (!primary_page_ || creation_time < primary_page_->navigation_start()) {
      return nullptr;
    }

    // Sub-frame resources have a null RFH when browser-side navigation is
    // enabled, so we can't perform the RFH check below for them.
    //
    // TODO(crbug.com/40216775): consider tracking GlobalRequestIDs for
    // sub-frame navigations in each PageLoadTracker, and performing a lookup
    // for sub-frames similar to the main-frame lookup above. Now we have
    // `active_pages_` in addition to `primary_page_`, and the following code
    // cannot handle sub-frames inside FencedFrames.
    if (blink::IsRequestDestinationFrame(request_destination)) {
      return primary_page_.get();
    }

    // This was originally a CHECK but it fails when the document load happened
    // after client certificate selection.
    if (!render_frame_host_or_null) {
      return nullptr;
    }

    // There is a race here: a completed resource for the previously committed
    // page can arrive after the new page has committed. In this case, we may
    // attribute the resource to the wrong page load. We do our best to guard
    // against this by verifying that the RFH for the resource matches the RFH
    // for the currently committed load, however there are cases where the same
    // RFH is used across page loads (same origin navigations, as well as some
    // cross-origin render-initiated navigations).
    //
    // TODO(crbug.com/40528374): use a DocumentId here instead, to eliminate
    // this race.
    return GetPageLoadTracker(render_frame_host_or_null);
  }
  return nullptr;
}

void MetricsWebContentsObserver::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  if (!ShouldTrackURL(resource_load_info.final_url)) {
    return;
  }

  PageLoadTracker* tracker = GetTrackerOrNullForRequest(
      request_id, render_frame_host, resource_load_info.request_destination,
      resource_load_info.load_timing_info.request_start);
  if (tracker) {
    // TODO(crbug.com/41318940): Fill in data reduction proxy fields when this
    // is available in the network service. int original_content_length =
    //     was_cached ? 0
    //                : data_reduction_proxy::util::EstimateOriginalBodySize(
    //                      request, lofi_decider);
    int original_content_length = 0;

    const blink::mojom::CommonNetworkInfoPtr& network_info =
        resource_load_info.network_info;
    ExtraRequestCompleteInfo extra_request_complete_info(
        url::SchemeHostPort(resource_load_info.final_url),
        network_info->remote_endpoint.value(),
        render_frame_host->GetFrameTreeNodeId(), resource_load_info.was_cached,
        resource_load_info.raw_body_bytes, original_content_length,
        resource_load_info.request_destination, resource_load_info.net_error,
        std::make_unique<net::LoadTimingInfo>(
            resource_load_info.load_timing_info));
    tracker->OnLoadedResource(extra_request_complete_info);
  }
}

void MetricsWebContentsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  if (PageLoadTracker* tracker = GetPageLoadTracker(render_frame_host)) {
    tracker->FrameReceivedUserActivation(render_frame_host);
  }
}

void MetricsWebContentsObserver::FrameDisplayStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_display_none) {
  if (PageLoadTracker* tracker = GetPageLoadTracker(render_frame_host)) {
    tracker->FrameDisplayStateChanged(render_frame_host, is_display_none);
  }
}

void MetricsWebContentsObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  if (PageLoadTracker* tracker = GetPageLoadTracker(render_frame_host)) {
    tracker->FrameSizeChanged(render_frame_host, frame_size);
  }
}

void MetricsWebContentsObserver::OnCookiesAccessed(
    content::NavigationHandle* navigation,
    const content::CookieAccessDetails& details) {
  PageLoadTracker* tracker = nullptr;
  if (navigation->GetParentFrame()) {
    // For subframe navigations, notify the main frame's tracker.
    tracker = GetPageLoadTracker(navigation->GetParentFrame());
  } else {
    // For uncommitted main frame navigations, find a tracker from
    // `provisional_loads_`.
    auto it = provisional_loads_.find(navigation);
    if (it != provisional_loads_.end()) {
      tracker = it->second.get();
    }
  }

  if (tracker) {
    OnCookiesAccessedImpl(*tracker, details);
  }
}

void MetricsWebContentsObserver::OnCookiesAccessed(
    content::RenderFrameHost* rfh,
    const content::CookieAccessDetails& details) {
  if (PageLoadTracker* tracker = GetPageLoadTracker(rfh)) {
    OnCookiesAccessedImpl(*tracker, details);
  }
}

void MetricsWebContentsObserver::OnCookiesAccessedImpl(
    PageLoadTracker& tracker,
    const content::CookieAccessDetails& details) {
  // TODO(altimin): Propagate `CookieAccessDetails` further.
  bool is_partitioned_access = base::ranges::all_of(
      details.cookie_access_result_list,
      [](const net::CookieWithAccessResult& cookie_with_access_result) {
        return cookie_with_access_result.cookie.IsPartitioned();
      });

  switch (details.type) {
    case content::CookieAccessDetails::Type::kRead:
      tracker.OnCookiesRead(details.url, details.first_party_url,
                            details.blocked_by_policy, details.is_ad_tagged,
                            details.cookie_setting_overrides,
                            is_partitioned_access);
      break;
    case content::CookieAccessDetails::Type::kChange:
      for (const auto& cookie_with_access_result :
           details.cookie_access_result_list) {
        tracker.OnCookieChange(details.url, details.first_party_url,
                               cookie_with_access_result.cookie,
                               details.blocked_by_policy, details.is_ad_tagged,
                               details.cookie_setting_overrides,
                               is_partitioned_access);
      }
      break;
  }
}

void MetricsWebContentsObserver::DidActivatePreviewedPage(
    base::TimeTicks activation_time) {
  // TODO(b:334709645): Investigate how nullptr cases happen.
  if (primary_page_) {
    primary_page_->DidActivatePreviewedPage(activation_time);
  }
}

void MetricsWebContentsObserver::OnStorageAccessed(
    content::RenderFrameHost* rfh,
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    StorageType storage_type) {
  if (PageLoadTracker* tracker = GetPageLoadTracker(rfh)) {
    tracker->OnStorageAccessed(url, first_party_url, blocked_by_policy,
                               storage_type);
  }
}

const PageLoadMetricsObserverDelegate&
MetricsWebContentsObserver::GetDelegateForCommittedLoad() {
  CHECK(primary_page_);
  return *primary_page_.get();
}

void MetricsWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    // Notify `primary_page_` that we are ready to commit a navigation to a
    // new page in the primary main frame.
    if (primary_page_) {
      primary_page_->ReadyToCommitNavigation(navigation_handle);
    }
  } else if (navigation_handle->IsInMainFrame()) {
    // For non-primary main frame, we notify the PageLoadTracker associated with
    // the RenderFrameHost that triggers the navigation.
    PageLoadTracker* tracker =
        GetPageLoadTracker(navigation_handle->GetRenderFrameHost());
    if (tracker) {
      tracker->ReadyToCommitNavigation(navigation_handle);
    }
  } else {
    // For subframe navigations, notify the PageLoadTracker associated with the
    // main frame.
    PageLoadTracker* tracker =
        GetPageLoadTracker(navigation_handle->GetParentFrame());
    if (tracker) {
      tracker->ReadyToCommitNavigation(navigation_handle);
    }
  }
}

void MetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    PageLoadTracker* tracker =
        GetPageLoadTracker(navigation_handle->GetParentFrame());
    if (tracker) {
      tracker->DidFinishSubFrameNavigation(navigation_handle);
    }
    return;
  }

  // Not all navigations trigger the WillStartNavigationRequest callback (for
  // example, navigations to about:blank). DidFinishNavigation is guaranteed to
  // be called for every navigation, so we also update has_navigated_ here, to
  // ensure it is set consistently for all navigations.
  // TODO(crbug.com/40216775): This flag seems broken for Prerender and
  // FencedFrames.
  has_navigated_ = true;

  std::unique_ptr<PageLoadTracker> navigation_handle_tracker(
      std::move(provisional_loads_[navigation_handle]));
  provisional_loads_.erase(navigation_handle);

  // Ignore same-document navigations.
  CHECK(navigation_handle->IsInMainFrame());
  if (navigation_handle->HasCommitted() &&
      navigation_handle->IsSameDocument()) {
    if (navigation_handle_tracker) {
      navigation_handle_tracker->StopTracking();
    }
    if (navigation_handle->IsInPrimaryMainFrame()) {
      if (primary_page_) {
        primary_page_->DidCommitSameDocumentNavigation(navigation_handle);
      }
    } else {
      // Handle the event for non-primary main frames, i.e., FencedFrames.
      PageLoadTracker* tracker =
          GetPageLoadTracker(navigation_handle->GetRenderFrameHost());
      if (tracker) {
        tracker->DidCommitSameDocumentNavigation(navigation_handle);
      }
    }
    return;
  }

  // Ignore internally generated aborts for navigations with HTTP responses that
  // don't commit, such as HTTP 204 responses and downloads.
  if (!navigation_handle->HasCommitted() &&
      navigation_handle->GetNetErrorCode() == net::ERR_ABORTED &&
      navigation_handle->GetResponseHeaders()) {
    if (navigation_handle_tracker) {
      navigation_handle_tracker->DidInternalNavigationAbort(navigation_handle);
      navigation_handle_tracker->StopTracking();
    }
    return;
  }

  if (navigation_handle->HasCommitted() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    // A new navigation is committing, so finalize and destroy the tracker for
    // the currently committed navigation.
    FinalizeCurrentlyCommittedLoad(navigation_handle,
                                   navigation_handle_tracker.get());

    if (primary_page_) {
      // Mark the current tracker as it sees a link navigation.
      ui::PageTransition transition = navigation_handle->GetPageTransition();
      if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK)) {
        primary_page_->RecordLinkNavigation();
      }
    }

    // Transfers the ownership of `primary_page_`. This `primary_page_`
    // might be reused later when restoring the page from the cache.
    // Note: back-forward cache doesn't support features that rely on
    // `active_pages_`, such as FencedFrames.
    MaybeStorePageLoadTrackerForBackForwardCache(navigation_handle,
                                                 std::move(primary_page_));

    // If `navigation_handle` already has an associated PageLoadTracker in
    // `inactive_pages_`, move it into `primary_page_`.
    if (MaybeActivatePageLoadTracker(navigation_handle)) {
      return;
    }
  }

  if (!navigation_handle_tracker) {
    return;
  }

  if (!ShouldTrackMainFrameNavigation(navigation_handle)) {
    navigation_handle_tracker->StopTracking();
    return;
  }

  if (navigation_handle->HasCommitted()) {
    navigation_handle_tracker->SetPageMainFrame(
        navigation_handle->GetRenderFrameHost());
    HandleCommittedNavigationForTrackedLoad(
        navigation_handle, std::move(navigation_handle_tracker));
  } else {
    HandleFailedNavigationForTrackedLoad(navigation_handle,
                                         std::move(navigation_handle_tracker));
  }
}

// Handle a pre-commit error. Navigations that result in an error page will be
// ignored.
void MetricsWebContentsObserver::HandleFailedNavigationForTrackedLoad(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PageLoadTracker> tracker) {
  const base::TimeTicks now = base::TimeTicks::Now();
  tracker->FailedProvisionalLoad(navigation_handle, now);

  const net::Error error = navigation_handle->GetNetErrorCode();

  // net::OK: This case occurs when the NavigationHandle finishes and reports
  // !HasCommitted(), but reports no net::Error. This represents the navigation
  // being stopped by the user before it was ready to commit.
  // net::ERR_ABORTED: An aborted provisional load has error net::ERR_ABORTED.
  const bool is_aborted_provisional_load =
      error == net::OK || error == net::ERR_ABORTED;

  // If is_aborted_provisional_load, the page end reason is not yet known, and
  // will be updated as additional information is available from subsequent
  // navigations.
  tracker->NotifyPageEnd(
      is_aborted_provisional_load ? END_OTHER : END_PROVISIONAL_LOAD_FAILED,
      UserInitiatedInfo::NotUserInitiated(), now, true);

  if (is_aborted_provisional_load) {
    aborted_provisional_loads_.push_back(std::move(tracker));
  }
}

void MetricsWebContentsObserver::HandleCommittedNavigationForTrackedLoad(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PageLoadTracker> tracker) {
  PageLoadTracker* raw_tracker = tracker.get();
  if (navigation_handle->IsInPrerenderedMainFrame()) {
    // The PageLoadTracker already exists when a main frame navigation after the
    // initial prerener navigation in a prerendering page is finished. Replace
    // the old page tracker with the new one.
    if (auto existing_tracker_iter =
            inactive_pages_.find(navigation_handle->GetRenderFrameHost());
        existing_tracker_iter != inactive_pages_.end()) {
      inactive_pages_.erase(existing_tracker_iter);
    }
    inactive_pages_.emplace(navigation_handle->GetRenderFrameHost(),
                            std::move(tracker));
  } else if (navigation_handle->IsInPrimaryMainFrame()) {
    primary_page_ = std::move(tracker);
    active_pages_.clear();
  } else {
    CHECK_EQ(navigation_handle->GetNavigatingFrameType(),
             content::FrameType::kFencedFrameRoot);
    // There may be an active tracker in the map if navigation happens on the
    // non-primary page. `emplace` operation below doesn't overwrite it, but
    // just fails. It results in destructing the moved tracker unexpectedly.
    // To avoid this problem, we ensure destructing existing tracker beforehand.
    auto it = active_pages_.find(navigation_handle->GetRenderFrameHost());
    if (it != active_pages_.end()) {
      active_pages_.erase(it);
    }

    active_pages_.emplace(navigation_handle->GetRenderFrameHost(),
                          std::move(tracker));
  }
  raw_tracker->Commit(navigation_handle);
  CHECK(raw_tracker->did_commit());

  for (auto& observer : lifecycle_observers_) {
    observer.OnCommit(raw_tracker);
  }

  auto* render_frame_host = navigation_handle->GetRenderFrameHost();
  const bool is_main_frame =
      render_frame_host && render_frame_host->GetParent() == nullptr;
  if (is_main_frame) {
    auto it = ukm_smoothness_data_.find(render_frame_host);
    if (it != ukm_smoothness_data_.end()) {
      raw_tracker->metrics_update_dispatcher()->SetUpSharedMemoryForSmoothness(
          render_frame_host, std::move(it->second));
      ukm_smoothness_data_.erase(it);
    }
  }

  // Send queued memory updates for the tracker.
  content::GlobalRenderFrameHostId rfh_id = render_frame_host->GetGlobalId();
  auto first_update_for_rfh = std::partition(
      queued_memory_updates_.begin(), queued_memory_updates_.end(),
      [rfh_id](const MemoryUpdate& update) {
        return update.routing_id != rfh_id;
      });
  if (first_update_for_rfh != queued_memory_updates_.end()) {
    raw_tracker->OnV8MemoryChanged(std::vector<MemoryUpdate>(
        first_update_for_rfh, queued_memory_updates_.end()));
    queued_memory_updates_.erase(first_update_for_rfh,
                                 queued_memory_updates_.end());
  }
}

void MetricsWebContentsObserver::MaybeStorePageLoadTrackerForBackForwardCache(
    content::NavigationHandle* next_navigation_handle,
    std::unique_ptr<PageLoadTracker> previously_committed_load) {
  TRACE_EVENT1("loading",
               "MetricsWebContentsObserver::"
               "MaybeRestorePageLoadTrackerForBackForwardCache",
               "next_navigation", next_navigation_handle);

  if (!previously_committed_load) {
    return;
  }

  content::RenderFrameHost* previous_frame = content::RenderFrameHost::FromID(
      next_navigation_handle->GetPreviousRenderFrameHostId());

  // The PageLoadTracker is associated with a bfcached document if:
  bool is_back_forward_cache =
      // 1. the frame being navigated away from was not already deleted
      previous_frame &&
      // 2. the previous frame is in the BFCache
      (previous_frame->GetLifecycleState() ==
       content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  if (!is_back_forward_cache) {
    return;
  }

  previously_committed_load->OnEnterBackForwardCache();
  inactive_pages_.emplace(previous_frame, std::move(previously_committed_load));
  for (auto& kv : active_pages_) {
    kv.second->OnEnterBackForwardCache();
    inactive_pages_.emplace(kv.first, std::move(kv.second));
  }
  active_pages_.clear();
}

bool MetricsWebContentsObserver::MaybeActivatePageLoadTracker(
    content::NavigationHandle* navigation_handle) {
  TRACE_EVENT1("loading",
               "MetricsWebContentsObserver::"
               "MaybeActivatePageLoadTracker",
               "navigation", navigation_handle);

  auto it = inactive_pages_.find(navigation_handle->GetRenderFrameHost());

  // There are some cases that the PageLoadTracker does not exist even if
  // `navigation_handle` is served from the back/forward cache. For example,
  // if a page is put into the cache before MetricsWebContents is created,
  // `inactive_pages_` is empty.
  if (it == inactive_pages_.end()) {
    return false;
  }

  active_pages_.clear();

  // This should be a back/forward cache or prerender navigation if we find
  // an inactive_page.
  CHECK(navigation_handle->IsServedFromBackForwardCache() ||
        navigation_handle->IsPrerenderedPageActivation());

  auto* primary_main_frame = navigation_handle->GetRenderFrameHost();
  primary_main_frame->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        // Skip RenderFrameHosts that aren't main frames.
        if (rfh != rfh->GetMainFrame()) {
          return;
        }
        auto it = inactive_pages_.find(rfh);
        if (it == inactive_pages_.end()) {
          return;
        }
        PageLoadTracker* tracker;
        if (rfh == primary_main_frame) {
          primary_page_ = std::move(it->second);
          tracker = primary_page_.get();
        } else {
          tracker = active_pages_.emplace(it->first, std::move(it->second))
                        .first->second.get();
        }
        inactive_pages_.erase(it);
        if (navigation_handle->IsServedFromBackForwardCache()) {
          tracker->OnRestoreFromBackForwardCache(navigation_handle);
        } else if (navigation_handle->IsPrerenderedPageActivation()) {
          tracker->DidActivatePrerenderedPage(navigation_handle);
        }
      });

  for (auto& observer : lifecycle_observers_) {
    observer.OnActivate(primary_page_.get());
  }

  return true;
}

void MetricsWebContentsObserver::FinalizeCurrentlyCommittedLoad(
    content::NavigationHandle* newly_committed_navigation,
    PageLoadTracker* newly_committed_navigation_tracker) {
  UserInitiatedInfo user_initiated_info =
      newly_committed_navigation_tracker
          ? newly_committed_navigation_tracker->user_initiated_info()
          : CreateUserInitiatedInfo(newly_committed_navigation);

  // Notify other loads that they may have been aborted by this committed
  // load. is_certainly_browser_timestamp is set to false because
  // NavigationStart() could be set in either the renderer or browser process.
  NotifyPageEndAllLoadsWithTimestamp(
      EndReasonForPageTransition(
          newly_committed_navigation->GetPageTransition()),
      user_initiated_info, newly_committed_navigation->NavigationStart(),
      /*is_certainly_browser_timestamp=*/false);

  if (primary_page_) {
    // Ensure that any pending update gets dispatched.
    primary_page_->metrics_update_dispatcher()->FlushPendingTimingUpdates();
  }
}

void MetricsWebContentsObserver::NavigationStopped() {
  // TODO(csharrison): Use a more user-initiated signal for STOP.
  NotifyPageEndAllLoads(END_STOP, UserInitiatedInfo::NotUserInitiated());
}

void MetricsWebContentsObserver::OnInputEvent(
    const blink::WebInputEvent& event) {
  // Ignore browser navigation or reload which comes with type Undefined.
  if (event.GetType() == blink::WebInputEvent::Type::kUndefined) {
    return;
  }

  // For now, we assume input events occur only in primary page.
  if (primary_page_) {
    primary_page_->OnInputEvent(event);
  }
}

void MetricsWebContentsObserver::FlushMetricsOnAppEnterBackground() {
  // Note that, while a call to FlushMetricsOnAppEnterBackground usually
  // indicates that the app is about to be backgrounded, there are cases where
  // the app may not end up getting backgrounded. Thus, we should not assume
  // anything about foreground / background state of the associated tab as part
  // of this method call.

  if (primary_page_) {
    primary_page_->FlushMetricsOnAppEnterBackground();
  }
  for (const auto& kv : active_pages_) {
    kv.second->FlushMetricsOnAppEnterBackground();
  }
  for (const auto& kv : inactive_pages_) {
    kv.second->FlushMetricsOnAppEnterBackground();
  }
  for (const auto& kv : provisional_loads_) {
    kv.second->FlushMetricsOnAppEnterBackground();
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    tracker->FlushMetricsOnAppEnterBackground();
  }
}

void MetricsWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end()) {
    return;
  }
  it->second->Redirect(navigation_handle);
}

void MetricsWebContentsObserver::DidUpdateNavigationHandleTiming(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end()) {
    return;
  }
  it->second->DidUpdateNavigationHandleTiming(navigation_handle);
}

void MetricsWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  if (web_contents_will_soon_be_destroyed_) {
    return;
  }

  bool was_in_foreground = in_foreground_;
  in_foreground_ = visibility == content::Visibility::VISIBLE;
  if (in_foreground_ == was_in_foreground) {
    return;
  }

  if (in_foreground_) {
    if (primary_page_) {
      primary_page_->PageShown();
    }
    for (const auto& kv : active_pages_) {
      kv.second->PageShown();
    }
    for (const auto& kv : provisional_loads_) {
      // Prerendered pages are always invisible regardless of the WebContents'
      // visibility status.
      if (!kv.first->IsInPrerenderedMainFrame()) {
        kv.second->PageShown();
      }
    }
  } else {
    if (primary_page_) {
      primary_page_->PageHidden();
    }
    for (const auto& kv : active_pages_) {
      kv.second->PageHidden();
    }
    for (const auto& kv : provisional_loads_) {
      if (!kv.first->IsInPrerenderedMainFrame()) {
        kv.second->PageHidden();
      }
    }
  }

  // As pages in back-forward cache are frozen and prerendered pages are always
  // invisible, `inactive_pages_` don't have to be iterated here.
}

// This will occur when the process for the main RenderFrameHost exits, either
// normally or from a crash. We eagerly log data from the last committed load if
// we have one.
void MetricsWebContentsObserver::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // Other code paths will be run for normal renderer shutdown. Note that we
  // sometimes get the STILL_RUNNING value on fast shutdown.
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }

  // RenderProcessGone is associated with the RenderFrameHost for the
  // currently committed load. We don't know if the pending navs or aborted
  // pending navs are associated w/ the render process that died, so we can't be
  // sure the info should propagate to them.
  const auto now = base::TimeTicks::Now();
  if (primary_page_) {
    primary_page_->NotifyPageEnd(END_RENDER_PROCESS_GONE,
                                 UserInitiatedInfo::NotUserInitiated(), now,
                                 true);
  }
  for (const auto& kv : active_pages_) {
    kv.second->NotifyPageEnd(END_RENDER_PROCESS_GONE,
                             UserInitiatedInfo::NotUserInitiated(), now, true);
  }

  // If this is a crash, eagerly log the aborted provisional loads and the
  // committed load. `provisional_loads_` don't need to be destroyed here
  // because their lifetime is tied to the NavigationHandle.
  primary_page_.reset();
  active_pages_.clear();
  aborted_provisional_loads_.clear();
}

void MetricsWebContentsObserver::NotifyPageEndAllLoads(
    PageEndReason page_end_reason,
    UserInitiatedInfo user_initiated_info) {
  NotifyPageEndAllLoadsWithTimestamp(page_end_reason, user_initiated_info,
                                     base::TimeTicks::Now(),
                                     /*is_certainly_browser_timestamp=*/true);
}

void MetricsWebContentsObserver::NotifyPageEndAllLoadsWithTimestamp(
    PageEndReason page_end_reason,
    UserInitiatedInfo user_initiated_info,
    base::TimeTicks timestamp,
    bool is_certainly_browser_timestamp) {
  if (primary_page_) {
    primary_page_->NotifyPageEnd(page_end_reason, user_initiated_info,
                                 timestamp, is_certainly_browser_timestamp);
  }
  for (const auto& kv : active_pages_) {
    kv.second->NotifyPageEnd(page_end_reason, user_initiated_info, timestamp,
                             is_certainly_browser_timestamp);
  }
  for (const auto& kv : provisional_loads_) {
    kv.second->NotifyPageEnd(page_end_reason, user_initiated_info, timestamp,
                             is_certainly_browser_timestamp);
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    if (tracker->IsLikelyProvisionalAbort(timestamp)) {
      tracker->UpdatePageEnd(page_end_reason, user_initiated_info, timestamp,
                             is_certainly_browser_timestamp);
    }
  }
  aborted_provisional_loads_.clear();
}

std::unique_ptr<PageLoadTracker>
MetricsWebContentsObserver::NotifyAbortedProvisionalLoadsNewNavigation(
    content::NavigationHandle* new_navigation,
    UserInitiatedInfo user_initiated_info) {
  // Prerendering navigations do not abort provisional loads in the active page.
  if (new_navigation->IsInPrerenderedMainFrame()) {
    return nullptr;
  }

  // If there are multiple aborted loads that can be attributed to this one,
  // just count the latest one for simplicity. Other loads will fall into the
  // OTHER bucket, though there shouldn't be very many.
  if (aborted_provisional_loads_.empty()) {
    return nullptr;
  }
  if (aborted_provisional_loads_.size() > 1) {
    RecordInternalError(ERR_NAVIGATION_SIGNALS_MULIPLE_ABORTED_LOADS);
  }

  std::unique_ptr<PageLoadTracker> last_aborted_load =
      std::move(aborted_provisional_loads_.back());
  aborted_provisional_loads_.pop_back();

  base::TimeTicks timestamp = new_navigation->NavigationStart();
  if (last_aborted_load->IsLikelyProvisionalAbort(timestamp)) {
    last_aborted_load->UpdatePageEnd(
        EndReasonForPageTransition(new_navigation->GetPageTransition()),
        user_initiated_info, timestamp, false);
  }

  aborted_provisional_loads_.clear();
  return last_aborted_load;
}

void MetricsWebContentsObserver::OnTimingUpdated(
    content::RenderFrameHost* render_frame_host,
    mojom::PageLoadTimingPtr timing,
    mojom::FrameMetadataPtr metadata,
    const std::vector<blink::UseCounterFeature>& new_features,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources,
    mojom::FrameRenderDataUpdatePtr render_data,
    mojom::CpuTimingPtr cpu_timing,
    mojom::InputTimingPtr input_timing_delta,
    const std::optional<blink::SubresourceLoadMetrics>&
        subresource_load_metrics,
    mojom::SoftNavigationMetricsPtr soft_navigation_metrics) {
  if (PageLoadTracker* tracker = GetPageLoadTrackerIfValid(render_frame_host)) {
    tracker->UpdateMetrics(
        render_frame_host, std::move(timing), std::move(metadata),
        std::move(new_features), resources, std::move(render_data),
        std::move(cpu_timing), std::move(input_timing_delta),
        subresource_load_metrics, std::move(soft_navigation_metrics));
  }
}

void MetricsWebContentsObserver::OnCustomUserTimingUpdated(
    content::RenderFrameHost* rfh,
    mojom::CustomUserTimingMarkPtr custom_timing) {
  // Buffer timing data before seinding to the tracker as the tracker may not
  // exist in some cases, in that case the buffered timings are sent next time.
  page_load_custom_timings_.push_back(std::move(custom_timing));
  if (PageLoadTracker* tracker = GetPageLoadTrackerIfValid(rfh)) {
    tracker->AddCustomUserTimings(std::move(page_load_custom_timings_));
  }
}

bool MetricsWebContentsObserver::DoesTimingUpdateHaveError(
    PageLoadTracker* tracker) {
  // TODO(crbug.com/40679416): Update page load metrics IPC validation to ues
  // mojo::ReportBadMessage.
  if (!tracker) {
    RecordInternalError(ERR_IPC_WITH_NO_RELEVANT_LOAD);
    return true;
  }

  if (!ShouldTrackURL(tracker->GetUrl())) {
    RecordInternalError(ERR_IPC_FROM_BAD_URL_SCHEME);
    return true;
  }

  return false;
}

void MetricsWebContentsObserver::UpdateTiming(
    mojom::PageLoadTimingPtr timing,
    mojom::FrameMetadataPtr metadata,
    const std::vector<blink::UseCounterFeature>& new_features,
    std::vector<mojom::ResourceDataUpdatePtr> resources,
    mojom::FrameRenderDataUpdatePtr render_data,
    mojom::CpuTimingPtr cpu_timing,
    mojom::InputTimingPtr input_timing_delta,
    const std::optional<blink::SubresourceLoadMetrics>&
        subresource_load_metrics,
    mojom::SoftNavigationMetricsPtr soft_navigation_metrics) {
  content::RenderFrameHost* render_frame_host =
      page_load_metrics_receivers_.GetCurrentTargetFrame();
  OnTimingUpdated(render_frame_host, std::move(timing), std::move(metadata),
                  new_features, resources, std::move(render_data),
                  std::move(cpu_timing), std::move(input_timing_delta),
                  subresource_load_metrics, std::move(soft_navigation_metrics));
}

void MetricsWebContentsObserver::AddCustomUserTiming(
    mojom::CustomUserTimingMarkPtr custom_timing) {
  content::RenderFrameHost* render_frame_host =
      page_load_metrics_receivers_.GetCurrentTargetFrame();
  OnCustomUserTimingUpdated(render_frame_host, std::move(custom_timing));
}

void MetricsWebContentsObserver::SetUpSharedMemoryForSmoothness(
    base::ReadOnlySharedMemoryRegion shared_memory) {
  content::RenderFrameHost* render_frame_host =
      page_load_metrics_receivers_.GetCurrentTargetFrame();
  const bool is_outermost_main_frame =
      render_frame_host->GetParentOrOuterDocument() == nullptr;
  if (!is_outermost_main_frame) {
    // TODO(crbug.com/40144214): Merge smoothness metrics from OOPIFs and
    // FencedFrames with the main-frame. Also need to check if FencedFrames
    // send this request correctly.
    return;
  }

  if (PageLoadTracker* tracker = GetPageLoadTracker(render_frame_host)) {
    tracker->metrics_update_dispatcher()->SetUpSharedMemoryForSmoothness(
        render_frame_host, std::move(shared_memory));
  } else {
    ukm_smoothness_data_.emplace(render_frame_host, std::move(shared_memory));
  }
}

bool MetricsWebContentsObserver::ShouldTrackMainFrameNavigation(
    content::NavigationHandle* navigation_handle) const {
  CHECK(navigation_handle->IsInMainFrame());
  CHECK(!navigation_handle->HasCommitted() ||
        !navigation_handle->IsSameDocument());
  if (!ShouldTrackURL(navigation_handle->GetURL())) {
    return false;
  }

  // The navigation served from the back-forward cache will use the previously
  // created tracker for the document.
  if (navigation_handle->IsServedFromBackForwardCache()) {
    return false;
  }

  // For a prerendering activation navigation, we will use a tracker in
  // `inactive_pages_` created in the initial prerendering navigation.
  if (navigation_handle->IsPrerenderedPageActivation()) {
    return false;
  }

  if (navigation_handle->HasCommitted()) {
    // Ignore Chrome error pages (e.g. No Internet connection).
    if (navigation_handle->IsErrorPage()) {
      return false;
    }

    // Ignore network error pages (e.g. 4xx, 5xx).
    int http_status_code = GetHttpStatusCode(navigation_handle);
    if (http_status_code > 0 &&
        (http_status_code < 200 || http_status_code >= 400)) {
      return false;
    }
  }

  return true;
}

bool MetricsWebContentsObserver::ShouldTrackURL(const GURL& url) const {
  if (embedder_interface_->IsNonTabWebUI()) {
    return true;
  }

  if (embedder_interface_->IsNewTabPageUrl(url)) {
    return true;
  }

  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kDataScheme) ||
         url.SchemeIs(url::kFileScheme);
}

void MetricsWebContentsObserver::OnBrowserFeatureUsage(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::UseCounterFeature>& new_features) {
  if (PageLoadTracker* tracker = GetPageLoadTracker(render_frame_host)) {
    tracker->metrics_update_dispatcher()->UpdateFeatures(render_frame_host,
                                                         new_features);
  } else {
    RecordInternalError(ERR_BROWSER_USAGE_WITH_NO_RELEVANT_LOAD);
  }
}

void MetricsWebContentsObserver::AddLifecycleObserver(
    MetricsLifecycleObserver* observer) {
  if (!lifecycle_observers_.HasObserver(observer)) {
    lifecycle_observers_.AddObserver(observer);
  }
}

void MetricsWebContentsObserver::RemoveLifecycleObserver(
    MetricsLifecycleObserver* observer) {
  lifecycle_observers_.RemoveObserver(observer);
}

void MetricsWebContentsObserver::OnPrefetchLikely() {
  // Prefetching can be triggered by speculation rules (by SpeculationHostImpl::
  // UpdateSpeculationCandidates()) or by NavigationPredictor, both of which
  // work only on behalf of a primary page.
  if (primary_page_) {
    primary_page_->OnPrefetchLikely();
  }
}

void MetricsWebContentsObserver::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  std::map<PageLoadTracker*, std::vector<MemoryUpdate>> per_tracker_updates;
  for (const MemoryUpdate& update : memory_updates) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(update.routing_id);
    if (!rfh) {
      continue;
    }
    PageLoadTracker* tracker = GetPageLoadTracker(rfh);
    if (tracker) {
      per_tracker_updates[tracker].push_back(update);
    } else {
      // If the load hasn't committed yet, then memory updates can't be sent
      // at this time, but will still need to be sent later. Queue the updates
      // in case `tracker` is null due to the navigation having not yet
      // completed, in which case the queued updates will be sent when
      // HandleCommittedNavigationForTrackedLoad is called.  Otherwise, they
      // will be ignored and cleared when `rfh` is deleted.
      queued_memory_updates_.push_back(update);
    }
  }

  for (const auto& map_pair : per_tracker_updates) {
    map_pair.first->OnV8MemoryChanged(map_pair.second);
  }
}

void MetricsWebContentsObserver::OnSharedStorageWorkletHostCreated(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    return;
  }

  if (PageLoadTracker* tracker = GetPageLoadTracker(rfh)) {
    tracker->OnSharedStorageWorkletHostCreated();
  }
}

void MetricsWebContentsObserver::OnSharedStorageSelectURLCalled(
    content::RenderFrameHost* main_rfh) {
  if (!main_rfh) {
    return;
  }

  if (PageLoadTracker* tracker = GetPageLoadTracker(main_rfh)) {
    tracker->OnSharedStorageSelectURLCalled();
  }
}

void MetricsWebContentsObserver::OnAdAuctionComplete(
    content::RenderFrameHost* rfh,
    bool is_server_auction,
    bool is_on_device_auction,
    content::AuctionResult result) {
  if (!rfh) {
    return;
  }

  if (PageLoadTracker* tracker = GetPageLoadTracker(rfh)) {
    tracker->OnAdAuctionComplete(is_server_auction, is_on_device_auction,
                                 result);
  }
}

base::TimeTicks MetricsWebContentsObserver::GetCreated() {
  return created_;
}

// This contains some bugs. RenderFrameHost::IsActive is not relevant to
// determine what members we have to search.
//
// There are some known wrong cases:
//
// 1. rfh->GetLifecycleState() == kReadyToBeDeleted && rfh is in active_pages_.
//    In this case, this method returns null. This case can occur, e.g.
//    navigation on a FF root node.
// 2. rfh->GetLifecycleState() == kActive && rfh is already deleted via
//    RenderFrameDeleted.
//    In this case, this method returns primary_page's PageLeadTracker. This
//    case can occur if the caller is FrameDeleted and, e.g. deletion of a FF
//    root node.
//
// This is mitigated by using GetPageLoadTracker.
//
// TODO(crbug.com/40216775): Use GetPageLoadTracker always.
PageLoadTracker* MetricsWebContentsObserver::GetPageLoadTrackerLegacy(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    return nullptr;
  }

  if (rfh->GetMainFrame()->IsActive()) {
    auto it = active_pages_.find(rfh->GetMainFrame());
    if (it != active_pages_.end()) {
      return it->second.get();
    }
    return primary_page_.get();
  }

  auto it = inactive_pages_.find(rfh->GetMainFrame());
  if (it != inactive_pages_.end()) {
    return it->second.get();
  }

  return nullptr;
}

PageLoadTracker* MetricsWebContentsObserver::GetPageLoadTracker(
    content::RenderFrameHost* rfh) {
  if (!rfh) {
    return nullptr;
  }

  if (rfh->GetPage().IsPrimary()) {
    return primary_page_.get();
  }

  {
    auto it = active_pages_.find(rfh->GetMainFrame());
    if (it != active_pages_.end()) {
      return it->second.get();
    }
  }

  {
    auto it = inactive_pages_.find(rfh->GetMainFrame());
    if (it != inactive_pages_.end()) {
      return it->second.get();
    }
  }

  return nullptr;
}

PageLoadTracker* MetricsWebContentsObserver::GetPageLoadTrackerIfValid(
    content::RenderFrameHost* render_frame_host) {
  // Replacing this call by GetPageLoadTracker breaks some tests.
  //
  // Note that if a PLMO only observes events at outermost page, misusing
  // primary page's PageLoadTracker for OnTimingUpdated is safe because
  // PageLoadTracker::UpdateMetrics forwards events unconditionally and
  // unmodified, and outermost page's MetricsUpdateDispatcher manages all
  // subframe's timing update.
  PageLoadTracker* tracker = GetPageLoadTrackerLegacy(render_frame_host);
  // We may receive notifications from frames that have been navigated away
  // from. In that case the PageLoadTracker is already destroyed in
  // DidFinishNavigation (unless it's stored in bfcache). We simply ignore them.
  if (!tracker && !render_frame_host->GetMainFrame()->IsActive()) {
    RecordInternalError(ERR_IPC_FROM_WRONG_FRAME);
    return nullptr;
  }

  const bool is_main_frame = (render_frame_host->GetParent() == nullptr);
  if (is_main_frame) {
    if (DoesTimingUpdateHaveError(tracker)) {
      return nullptr;
    }
  } else if (!tracker) {
    RecordInternalError(ERR_SUBFRAME_IPC_WITH_NO_RELEVANT_LOAD);
  }

  return tracker;
}

PageLoadTracker* MetricsWebContentsObserver::GetAncestralAlivePageLoadTracker(
    content::RenderFrameHost* rfh) {
  content::RenderFrameHost* ancestor = rfh;
  while (ancestor) {
    ancestor = ancestor->GetMainFrame();

    if (PageLoadTracker* tracker = GetPageLoadTracker(rfh)) {
      return tracker;
    }

    ancestor = ancestor->GetParentOrOuterDocument();
  }

  return nullptr;
}

PageLoadMetricsMemoryTracker* MetricsWebContentsObserver::GetMemoryTracker()
    const {
  return embedder_interface_->GetMemoryTrackerForBrowserContext(
      web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MetricsWebContentsObserver);

}  // namespace page_load_metrics
