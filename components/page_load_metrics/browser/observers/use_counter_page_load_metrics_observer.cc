// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom.h"

using FeatureType = blink::mojom::UseCounterFeatureType;
using UkmFeatureList = UseCounterMetricsRecorder::UkmFeatureList;
using WebFeature = blink::mojom::WebFeature;
using CSSSampleId = blink::mojom::CSSSampleId;
using PermissionsPolicyFeature = blink::mojom::PermissionsPolicyFeature;
using UserAgentOverrideHistogram =
    blink::UserAgentOverride::UserAgentOverrideHistogram;

#define FEATURE_HISTOGRAM_NAME(name, is_in_fenced_frames)     \
  is_in_fenced_frames ? "Blink.UseCounter.FencedFrames." name \
                      : "Blink.UseCounter." name

namespace {

// It's always recommended to use the deprecation API in blink. If the feature
// was logged from the browser (or from both blink and the browser) where the
// deprecation API is not available, use this method for the console warning.
// Note that this doesn't generate the deprecation report.
void PossiblyWarnFeatureDeprecation(content::RenderFrameHost* rfh,
                                    WebFeature feature) {
  switch (feature) {
    case WebFeature::kDownloadInSandbox:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Download is disallowed. The frame initiating or instantiating the "
          "download is sandboxed, but the flag ‘allow-downloads’ is not set. "
          "See https://www.chromestatus.com/feature/5706745674465280 for more "
          "details.");
      return;
    case WebFeature::kDownloadInAdFrameWithoutUserGesture:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "[Intervention] Download in ad frame without user activation is "
          "not allowed. See "
          "https://www.chromestatus.com/feature/6311883621531648 for more "
          "details.");
      return;

    default:
      return;
  }
}

template <size_t N>
bool TestAndSet(std::bitset<N>& bitset,
                blink::UseCounterFeature::EnumValue value) {
  bool has_record = bitset.test(value);
  bitset.set(value);
  return has_record;
}

}  // namespace

UseCounterMetricsRecorder::UseCounterMetricsRecorder(bool is_in_fenced_frame)
    : uma_features_(AtMostOnceEnumUmaDeferrer<blink::mojom::WebFeature>(
          FEATURE_HISTOGRAM_NAME("Features", is_in_fenced_frame))),
      uma_main_frame_features_(
          AtMostOnceEnumUmaDeferrer<blink::mojom::WebFeature>(
              FEATURE_HISTOGRAM_NAME("MainFrame.Features",
                                     is_in_fenced_frame))),
      uma_css_properties_(AtMostOnceEnumUmaDeferrer<blink::mojom::CSSSampleId>(
          FEATURE_HISTOGRAM_NAME("CSSProperties", is_in_fenced_frame))),
      uma_animated_css_properties_(
          AtMostOnceEnumUmaDeferrer<blink::mojom::CSSSampleId>(
              FEATURE_HISTOGRAM_NAME("AnimatedCSSProperties",
                                     is_in_fenced_frame))),
      uma_permissions_policy_violation_enforce_(
          AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>(
              FEATURE_HISTOGRAM_NAME("PermissionsPolicy.Violation.Enforce",
                                     is_in_fenced_frame))),
      uma_permissions_policy_allow2_(
          AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>(
              FEATURE_HISTOGRAM_NAME("PermissionsPolicy.Allow2",
                                     is_in_fenced_frame))),
      uma_permissions_policy_header2_(
          AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>(
              FEATURE_HISTOGRAM_NAME("PermissionsPolicy.Header2",
                                     is_in_fenced_frame))),
      uma_user_agent_override_(
          AtMostOnceEnumUmaDeferrer<
              blink::UserAgentOverride::UserAgentOverrideHistogram>(
              FEATURE_HISTOGRAM_NAME("UserAgentOverride",
                                     is_in_fenced_frame))) {}

UseCounterMetricsRecorder::~UseCounterMetricsRecorder() = default;

void UseCounterMetricsRecorder::AssertNoMetricsRecordedOrDeferred() {
  // Verify that no feature usage is observed before commit
  DCHECK_EQ(uma_features_.recorded_or_deferred().count(), 0ul);
  DCHECK_EQ(uma_main_frame_features_.recorded_or_deferred().count(), 0ul);
  DCHECK_EQ(uma_css_properties_.recorded_or_deferred().count(), 0ul);
  DCHECK_EQ(uma_animated_css_properties_.recorded_or_deferred().count(), 0ul);
  DCHECK_EQ(
      uma_permissions_policy_violation_enforce_.recorded_or_deferred().count(),
      0ul);
  DCHECK_EQ(uma_permissions_policy_allow2_.recorded_or_deferred().count(), 0ul);
  DCHECK_EQ(uma_permissions_policy_header2_.recorded_or_deferred().count(),
            0ul);

  DCHECK_EQ(ukm_features_recorded_.count(), 0ul);
  DCHECK_EQ(webdev_metrics_ukm_features_recorded_.count(), 0ul);
}

void UseCounterMetricsRecorder::RecordUkmPageVisits(
    ukm::SourceId ukm_source_id) {
  auto web_feature_page_visit =
      static_cast<blink::UseCounterFeature::EnumValue>(WebFeature::kPageVisits);

  ukm::builders::Blink_UseCounter(ukm_source_id)
      .SetFeature(web_feature_page_visit)
      .SetIsMainFrameFeature(1)
      .Record(ukm::UkmRecorder::Get());
  ukm_features_recorded_.set(web_feature_page_visit);
}

void UseCounterMetricsRecorder::DisableDeferAndFlush() {
  uma_features_.DisableDeferAndFlush();
  uma_main_frame_features_.DisableDeferAndFlush();
  uma_css_properties_.DisableDeferAndFlush();
  uma_animated_css_properties_.DisableDeferAndFlush();
  uma_permissions_policy_violation_enforce_.DisableDeferAndFlush();
  uma_permissions_policy_allow2_.DisableDeferAndFlush();
  uma_permissions_policy_header2_.DisableDeferAndFlush();
  uma_user_agent_override_.DisableDeferAndFlush();
}

void UseCounterMetricsRecorder::RecordOrDeferUseCounterFeature(
    content::RenderFrameHost* rfh,
    const blink::UseCounterFeature& feature) {
  switch (feature.type()) {
    case FeatureType::kWebFeature: {
      WebFeature sample = static_cast<WebFeature>(feature.value());

      if (!uma_features_.IsRecordedOrDeferred(sample)) {
        PossiblyWarnFeatureDeprecation(rfh, sample);
        uma_features_.RecordOrDefer(sample);
      }
    } break;
    // There are about 600 enums, so the memory required for a vector
    // histogram is about 600 * 8 bytes = 5KB 50% of the time there are about
    // 100 CSS properties recorded per page load. Storage in sparce
    // histogram entries are 48 bytes instead of 8 bytes so the memory
    // required for a sparse histogram is about 100 * 48 bytes = 5KB. On top
    // there will be std::map overhead and the acquire/release of a
    // base::Lock to protect the map during each update. Overall it is still
    // better to use a vector histogram here since it is faster to access
    // and merge and uses about same amount of memory.
    case FeatureType::kCssProperty:
      uma_css_properties_.RecordOrDefer(
          static_cast<CSSSampleId>(feature.value()));
      break;
    case FeatureType::kAnimatedCssProperty:
      uma_animated_css_properties_.RecordOrDefer(
          static_cast<CSSSampleId>(feature.value()));
      break;
    case FeatureType::kPermissionsPolicyViolationEnforce:
      uma_permissions_policy_violation_enforce_.RecordOrDefer(
          static_cast<PermissionsPolicyFeature>(feature.value()));
      break;
    case FeatureType::kPermissionsPolicyHeader:
      uma_permissions_policy_header2_.RecordOrDefer(
          static_cast<PermissionsPolicyFeature>(feature.value()));
      break;
    case FeatureType::kPermissionsPolicyIframeAttribute:
      uma_permissions_policy_allow2_.RecordOrDefer(
          static_cast<PermissionsPolicyFeature>(feature.value()));
      break;
    case FeatureType::kUserAgentOverride:
      uma_user_agent_override_.RecordOrDefer(
          static_cast<UserAgentOverrideHistogram>(feature.value()));
      break;
  }
}

void UseCounterMetricsRecorder::RecordOrDeferMainFrameWebFeature(
    content::RenderFrameHost* rfh,
    blink::mojom::WebFeature web_feature) {
  // Don't check if the primary main frame of not, but just ignore sub-frame
  // cases as we record metrics also for non-primary main frame, e.g.
  // FencedFrames, if the instance is bound with the FencedFrames page.
  if (rfh->GetParent())
    return;

  uma_main_frame_features_.RecordOrDefer(web_feature);
}

void UseCounterMetricsRecorder::RecordUkmFeatures(ukm::SourceId ukm_source_id) {
  for (WebFeature web_feature : GetAllowedUkmFeatures()) {
    auto feature_enum_value =
        static_cast<blink::UseCounterFeature::EnumValue>(web_feature);
    if (!uma_features_.IsRecordedOrDeferred(web_feature))
      continue;

    if (TestAndSet(ukm_features_recorded_, feature_enum_value))
      continue;

    ukm::builders::Blink_UseCounter(ukm_source_id)
        .SetFeature(feature_enum_value)
        .SetIsMainFrameFeature(
            uma_main_frame_features_.IsRecordedOrDeferred(web_feature))
        .Record(ukm::UkmRecorder::Get());
  }
  for (WebFeature web_feature : GetAllowedWebDevMetricsUkmFeatures()) {
    auto feature_enum_value =
        static_cast<blink::UseCounterFeature::EnumValue>(web_feature);
    if (!uma_features_.IsRecordedOrDeferred(web_feature))
      continue;

    if (TestAndSet(webdev_metrics_ukm_features_recorded_, feature_enum_value))
      continue;

    ukm::builders::Blink_DeveloperMetricsRare(ukm_source_id)
        .SetFeature(feature_enum_value)
        .SetIsMainFrameFeature(
            uma_main_frame_features_.IsRecordedOrDeferred(web_feature))
        .Record(ukm::UkmRecorder::Get());
  }
}

UseCounterPageLoadMetricsObserver::UseCounterPageLoadMetricsObserver() =
    default;

UseCounterPageLoadMetricsObserver::~UseCounterPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  recorder_ = std::make_unique<UseCounterMetricsRecorder>(
      /* is_in_fenced_frame */ false);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Continue even if this instance is bound to a FencedFrames page. In such
  // cases, report metrics prefixed by "Blink.UseCounter.FencedFrames".
  recorder_ = std::make_unique<UseCounterMetricsRecorder>(
      /* is_in_fenced_frame */ true);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Works as same as non prerendered case. UMAs/UKMs are not recorded for
  // cancelled prerendering.
  recorder_ = std::make_unique<UseCounterMetricsRecorder>(
      /* is_in_fenced_frame */ false);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  recorder_->AssertNoMetricsRecordedOrDeferred();

  content::RenderFrameHost* rfh = navigation_handle->GetRenderFrameHost();

  // Each Page including FencedFrames Page will report with the SourceId that is
  // bound with the outermost main frame.
  if (!IsInPrerenderingBeforeActivation()) {
    recorder_->RecordUkmPageVisits(GetDelegate().GetPageUkmSourceId());
    recorder_->DisableDeferAndFlush();
  }

  recorder_->RecordOrDeferMainFrameWebFeature(rfh, WebFeature::kPageVisits);
  auto web_feature_page_visit =
      static_cast<blink::UseCounterFeature::EnumValue>(WebFeature::kPageVisits);
  recorder_->RecordOrDeferUseCounterFeature(
      rfh, {FeatureType::kWebFeature, web_feature_page_visit});

  auto css_total_pages_measured =
      static_cast<blink::UseCounterFeature::EnumValue>(
          CSSSampleId::kTotalPagesMeasured);
  recorder_->RecordOrDeferUseCounterFeature(
      rfh, {FeatureType::kCssProperty, css_total_pages_measured});
  recorder_->RecordOrDeferUseCounterFeature(
      rfh, {FeatureType::kAnimatedCssProperty, css_total_pages_measured});

  return CONTINUE_OBSERVING;
}

void UseCounterPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  auto begin = base::TimeTicks::Now();

  recorder_->RecordUkmPageVisits(GetDelegate().GetPageUkmSourceId());
  recorder_->DisableDeferAndFlush();

  auto end = base::TimeTicks::Now();
  base::TimeDelta elapsed = end - begin;
  // Records duration of DisableDeferAndFlush.
  base::UmaHistogramTimes(
      "PageLoad.Clients.UseCounter.Experimental."
      "MetricsReplayAtActivationDuration",
      elapsed);
}

void UseCounterPageLoadMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  for (const blink::UseCounterFeature& feature : features) {
    if (feature.type() == FeatureType::kWebFeature) {
      recorder_->RecordOrDeferMainFrameWebFeature(
          rfh, static_cast<WebFeature>(feature.value()));
    }
    recorder_->RecordOrDeferUseCounterFeature(rfh, feature);
  }
}

void UseCounterPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (IsInPrerenderingBeforeActivation())
    return;

  recorder_->RecordUkmFeatures(GetDelegate().GetPageUkmSourceId());
}

void UseCounterPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  if (IsInPrerenderingBeforeActivation())
    return;

  recorder_->RecordUkmFeatures(GetDelegate().GetPageUkmSourceId());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (IsInPrerenderingBeforeActivation())
    return CONTINUE_OBSERVING;

  recorder_->RecordUkmFeatures(GetDelegate().GetPageUkmSourceId());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  if (mime_type == "image/svg+xml")
    return CONTINUE_OBSERVING;
  return PageLoadMetricsObserver::ShouldObserveMimeType(mime_type);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

bool UseCounterPageLoadMetricsObserver::IsInPrerenderingBeforeActivation()
    const {
  return (GetDelegate().GetPrerenderingState() ==
          page_load_metrics::PrerenderingState::kInPrerendering);
}
