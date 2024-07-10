// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/third_party_metrics_observer.h"

#include "base/containers/enum_set.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace {

// The maximum number of subframes that we've recorded timings for that we can
// keep track of in memory.
const int kMaxRecordedFrames = 50;

bool IsSameSite(const url::Origin& origin1, const url::Origin& origin2) {
  return origin1.scheme() == origin2.scheme() &&
         net::registry_controlled_domains::SameDomainOrHost(
             origin1, origin2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsSameSite(const GURL& url1, const GURL& url2) {
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

ThirdPartyMetricsObserver::ThirdPartyMetricsObserver() = default;
ThirdPartyMetricsObserver::~ThirdPartyMetricsObserver() = default;
ThirdPartyMetricsObserver::ThirdPartyInfo::ThirdPartyInfo() = default;
ThirdPartyMetricsObserver::ThirdPartyInfo::ThirdPartyInfo(
    const ThirdPartyInfo&) = default;

const char* ThirdPartyMetricsObserver::GetObserverName() const {
  static const char kName[] = "ThirdPartyMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ThirdPartyMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40222513): Handle Prerendering cases.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ThirdPartyMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // FrameReceivedUserActivation, OnLoadedResource, OnCookies{Read|Change}, and
  // OnStorageAccessed need the observer-side forwarding.
  return FORWARD_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ThirdPartyMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // The browser may come back, but there is no guarantee. To be safe, record
  // what we have now and ignore future changes to this navigation.
  RecordMetrics(timing);
  return STOP_OBSERVING;
}

void ThirdPartyMetricsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  bool is_third_party = false;
  auto* third_party_info = GetThirdPartyInfo(
      render_frame_host->GetLastCommittedURL(),
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetPrimaryMainFrame()
          ->GetLastCommittedURL(),
      is_third_party);

  // Update the activation status and record use counters as necessary.
  if (is_third_party && third_party_info != nullptr &&
      !third_party_info->activation) {
    third_party_info->activation = true;
    RecordUseCounters(AccessType::kMaxValue, third_party_info);
  }
}

void ThirdPartyMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordMetrics(timing);
}

void ThirdPartyMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (third_party_font_loaded_ ||
      extra_request_complete_info.request_destination !=
          network::mojom::RequestDestination::kFont) {
    return;
  }

  third_party_font_loaded_ = !IsSameSite(
      GetDelegate().GetUrl(), extra_request_complete_info.final_url.GetURL());
}

void ThirdPartyMetricsObserver::OnCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  OnCookieOrStorageAccess(url, first_party_url, blocked_by_policy,
                          AccessType::kCookieRead);
}

void ThirdPartyMetricsObserver::OnCookieChange(
    const GURL& url,
    const GURL& first_party_url,
    const net::CanonicalCookie& cookie,
    bool blocked_by_policy,
    bool is_ad_tagged,
    const net::CookieSettingOverrides& cookie_setting_overrides,
    bool is_partitioned_access) {
  OnCookieOrStorageAccess(url, first_party_url, blocked_by_policy,
                          AccessType::kCookieWrite);
}

// TODO(crbug.com/40144431): It would be simpler to just pass in ThirdPartyInfo
// and set the bits appropriately, but because this is called every time an
// access is made, that would mean re-calling old accesses.  This could be fixed
// by calling this only when the page is removed or when backgrounded.
void ThirdPartyMetricsObserver::RecordUseCounters(
    AccessType access_type,
    const ThirdPartyInfo* third_party_info) {
  std::vector<blink::mojom::WebFeature> third_party_storage_features;

  // We only record access/activation if the third_party_info didn't overflow.
  if (third_party_info != nullptr) {
    // Record any sort of access.
    if (third_party_info->access_types.any()) {
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyAccess);
    }
    // Record any sort of activation.
    if (third_party_info->activation) {
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyActivation);
    }
    // Record the combination of the above two
    if (third_party_info->access_types.any() && third_party_info->activation) {
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyAccessAndActivation);
    }
  }

  // Record the specific type of access, if appropriate.
  switch (access_type) {
    case AccessType::kCookieRead:
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyCookieRead);
      break;
    case AccessType::kCookieWrite:
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyCookieWrite);
      break;
    case AccessType::kLocalStorage:
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyLocalStorage);
      break;
    case AccessType::kSessionStorage:
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartySessionStorage);
      break;
    case AccessType::kFileSystem:
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyFileSystem);
      break;
    case AccessType::kIndexedDb:
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyIndexedDb);
      break;
    case AccessType::kCacheStorage:
      third_party_storage_features.push_back(
          blink::mojom::WebFeature::kThirdPartyCacheStorage);
      break;
    default:
      // No feature usage recorded for storage types without a use counter.
      // Also nothing reported for non storage access.
      break;
  }

  // Report the feature usage if there's anything to report.
  if (third_party_storage_features.size() > 0) {
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        GetDelegate().GetWebContents()->GetPrimaryMainFrame(),
        std::move(third_party_storage_features));
  }
}

void ThirdPartyMetricsObserver::OnStorageAccessed(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    page_load_metrics::StorageType storage_type) {
  OnCookieOrStorageAccess(url, first_party_url, blocked_by_policy,
                          StorageTypeToAccessType(storage_type));
}

void ThirdPartyMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  // A RenderFrameHost is navigating. Since this is a new navigation we want to
  // capture its paint timing. Remove the RFH from the list of recorded frames.
  // This is guaranteed to be called before receiving the first paint update for
  // the navigation.
  recorded_frames_.erase(navigation_handle->GetRenderFrameHost());
}

void ThirdPartyMetricsObserver::OnRenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  recorded_frames_.erase(render_frame_host);
}

void ThirdPartyMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Filter out top-frames
  if (!subframe_rfh) {
    return;
  }

  // If we've reached the frame limit and haven't recorded any metrics for this
  // RFH before, then don't continue on.
  auto recorded_frame_events = recorded_frames_.find(subframe_rfh);
  if (recorded_frame_events == recorded_frames_.end() &&
      recorded_frames_.size() >= kMaxRecordedFrames) {
    return;
  }

  // Filter out first-party frames.
  content::RenderFrameHost* top_frame =
      GetDelegate().GetWebContents()->GetPrimaryMainFrame();
  if (!top_frame) {
    return;
  }

  const url::Origin& top_frame_origin = top_frame->GetLastCommittedOrigin();
  const url::Origin& subframe_origin = subframe_rfh->GetLastCommittedOrigin();
  if (IsSameSite(top_frame_origin, subframe_origin)) {
    return;
  }

  bool should_record_opaque_subframe_time =
      !top_frame_origin.opaque() && subframe_origin.opaque();

  TimingEventTypeEnumSet newly_recorded_event_types;

  if (timing.paint_timing->first_contentful_paint &&
      page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    // Filter out navigations that we've already recorded.
    if (recorded_frame_events == recorded_frames_.end() ||
        !recorded_frame_events->second.Has(
            TimingEventType::kFirstContentfulPaint)) {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.Clients.ThirdParty.Frames."
          "NavigationToFirstContentfulPaint3",
          timing.paint_timing->first_contentful_paint.value());
      if (should_record_opaque_subframe_time) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Clients.ThirdParty.Frames.Opaque."
            "NavigationToFirstContentfulPaint",
            timing.paint_timing->first_contentful_paint.value());
      }
      newly_recorded_event_types.Put(TimingEventType::kFirstContentfulPaint);
    }
  }

  const page_load_metrics::ContentfulPaintTimingInfo&
      cross_site_sub_frame_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .CrossSiteSubframesLargestContentfulPaint();
  if (cross_site_sub_frame_largest_contentful_paint.ContainsValidTime() &&
      page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          cross_site_sub_frame_largest_contentful_paint.Time(),
          GetDelegate())) {
    // Filter out navigations that we've already recorded.
    if (recorded_frame_events == recorded_frames_.end() ||
        !recorded_frame_events->second.Has(
            TimingEventType::kLargestContentfulPaint)) {
      if (should_record_opaque_subframe_time) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Clients.ThirdParty.Frames.Opaque."
            "NavigationToLargestContentfulPaint",
            cross_site_sub_frame_largest_contentful_paint.Time().value());
      }
      // Note: We may not have actually recorded a histogram value at this
      // point, but indicate that we did so that the LCP metric here has values
      // that correspond to the FCP metric recorded above.
      newly_recorded_event_types.Put(TimingEventType::kLargestContentfulPaint);
    }
  }

  if (!newly_recorded_event_types.empty()) {
    if (recorded_frame_events == recorded_frames_.end()) {
      recorded_frames_[subframe_rfh] = newly_recorded_event_types;
    } else {
      recorded_frames_[subframe_rfh] = base::Union(
          recorded_frame_events->second, newly_recorded_event_types);
    }
  }
}

ThirdPartyMetricsObserver::ThirdPartyInfo*
ThirdPartyMetricsObserver::GetThirdPartyInfo(const GURL& url,
                                             const GURL& first_party_url,
                                             bool& is_third_party) {
  is_third_party = false;

  // TODO(csharrison): Optimize the domain lookup.
  // Note: If either |url| or |first_party_url| is empty, SameDomainOrHost will
  // return false, and function execution will continue because it is considered
  // 3rd party. Since |first_party_url| is actually the |site_for_cookies|, this
  // will happen e.g. for a 3rd party iframe on document.cookie access.
  if (!url.is_valid() || IsSameSite(url, first_party_url)) {
    return nullptr;
  }

  std::string registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  // |registrable_domain| can be empty e.g. if |url| is on an IP address, or the
  // domain is itself a TLD, or it's a file URL (in which case it has no host),
  // etc. See comment for GetDomainAndRegistry() in
  // //net/base/registry_controlled_domains/registry_controlled_domains.h.
  if (registrable_domain.empty()) {
    if (url.has_host()) {
      registrable_domain = url.host();
    } else {
      return nullptr;
    }
  }

  // If we haven't returned by this point, this is a third party access.
  is_third_party = true;

  GURL representative_url(
      base::StrCat({url.scheme(), "://", registrable_domain, "/"}));
  auto it = all_third_party_info_.find(representative_url);
  if (it == all_third_party_info_.end() &&
      all_third_party_info_.size() < 1000) {  // Bound growth.
    it = all_third_party_info_.emplace(url, ThirdPartyInfo()).first;
  }
  // If there's no valid iterator, we've gone over the size limit for the map.
  // TODO(crbug.com/40144431): We probably want UMA to let us know how often we
  // might be underreporting.
  return (it == all_third_party_info_.end() ? nullptr : &it->second);
}

void ThirdPartyMetricsObserver::OnCookieOrStorageAccess(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    AccessType access_type) {
  DCHECK(access_type != AccessType::kUnknown);
  if (blocked_by_policy) {
    should_record_metrics_ = false;
    return;
  }

  bool is_third_party = false;
  auto* third_party_info =
      GetThirdPartyInfo(url, first_party_url, is_third_party);
  if (!is_third_party) {
    return;
  }
  if (third_party_info != nullptr) {
    third_party_info->access_types[static_cast<size_t>(access_type)] = true;
  }

  // Record the use counters as necessary.
  RecordUseCounters(access_type, third_party_info);
}

void ThirdPartyMetricsObserver::RecordMetrics(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  if (!should_record_metrics_) {
    return;
  }

  int cookie_origin_reads = 0;

  for (auto it : all_third_party_info_) {
    const ThirdPartyInfo& tpi = it.second;
    if (tpi.access_types[static_cast<size_t>(AccessType::kCookieRead)]) {
      ++cookie_origin_reads;
    }
  }

  UMA_HISTOGRAM_COUNTS_1000("PageLoad.Clients.ThirdParty.Origins.CookieRead2",
                            cookie_origin_reads);

  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (third_party_font_loaded_ &&
      all_frames_largest_contentful_paint.ContainsValidTime() &&
      all_frames_largest_contentful_paint.TextOrImage() ==
          page_load_metrics::ContentfulPaintTimingInfo::
              LargestContentTextOrImage::kText &&
      WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.ThirdParty.PaintTiming."
        "NavigationToLargestContentfulPaint.HasThirdPartyFont",
        all_frames_largest_contentful_paint.Time().value());
  }
}

ThirdPartyMetricsObserver::AccessType
ThirdPartyMetricsObserver::StorageTypeToAccessType(
    page_load_metrics::StorageType storage_type) {
  switch (storage_type) {
    case page_load_metrics::StorageType::kLocalStorage:
      return AccessType::kLocalStorage;
    case page_load_metrics::StorageType::kSessionStorage:
      return AccessType::kSessionStorage;
    case page_load_metrics::StorageType::kFileSystem:
      return AccessType::kFileSystem;
    case page_load_metrics::StorageType::kIndexedDb:
      return AccessType::kIndexedDb;
    case page_load_metrics::StorageType::kCacheStorage:
      return AccessType::kCacheStorage;
    default:
      return AccessType::kUnknown;
  }
}
