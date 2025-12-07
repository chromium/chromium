// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/paid_content_page_load_metrics_observer.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom.h"

PaidContentPageLoadMetricsObserver::PaidContentPageLoadMetricsObserver() =
    default;

PaidContentPageLoadMetricsObserver::~PaidContentPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PaidContentPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!base::FeatureList::IsEnabled(
          page_load_metrics::features::kPaidContentMetricsObserver)) {
    return STOP_OBSERVING;
  }
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PaidContentPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return STOP_OBSERVING;
  }

  if (navigation_handle->IsSameDocument()) {
    return CONTINUE_OBSERVING;
  }

  // The PaidContentMetadataObserver only sends when there is paid content,
  // so we initialize to false.
  has_paid_content_ = false;
  // Reset the receiver and remote on each commit to ensure clean state.
  receiver_.reset();
  registry_.reset();

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PaidContentPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This observer is not interested in fenced frames.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PaidContentPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This observer is not interested in prerendering.
  return STOP_OBSERVING;
}

void PaidContentPageLoadMetricsObserver::OnPaidContentMetadataChanged(
    bool has_paid_content) {
  has_paid_content_ = has_paid_content;
}

void PaidContentPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // If we are already bound, there is nothing to do.
  if (receiver_.is_bound()) {
    return;
  }

  content::WebContents* web_contents = GetDelegate().GetWebContents();
  if (!web_contents) {
    return;
  }

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  if (!rfh || !rfh->IsRenderFrameLive()) {
    return;
  }
  rfh->GetRemoteInterfaces()->GetInterface(
      registry_.BindNewPipeAndPassReceiver());

  // The registry is associated with the RenderFrameHost, so a disconnect means
  // the RFH is gone.
  registry_.set_disconnect_handler(
      base::BindOnce(&PaidContentPageLoadMetricsObserver::
                         OnPaidContentMetadataObserverDisconnect,
                     base::Unretained(this)));

  registry_->AddPaidContentMetadataObserver(
      receiver_.BindNewPipeAndPassRemote());
}

void PaidContentPageLoadMetricsObserver::
    OnPaidContentMetadataObserverDisconnect() {
  // The registry can be null in tests.
  if (registry_.is_bound()) {
    registry_.reset();
  }
  receiver_.reset();
}

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PaidContentState)
enum class PaidContentState {
  kUnknown = 0,
  kHasPaidContent = 1,
  kNoPaidContentFound = 2,
  kMaxValue = kNoPaidContentFound,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:PaidContentState)

const char kHistogramPaidContentPageLoad[] = "PageLoad.PaidContent.State";

void RecordPaidContentPageLoad(
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate,
    std::optional<bool> has_paid_content) {
  PaidContentState state = PaidContentState::kUnknown;
  // TODO(gklassen): Consider adding support for isAccessibleForFree=true as new
  // enum value.
  if (has_paid_content.has_value()) {
    state = has_paid_content.value() ? PaidContentState::kHasPaidContent
                                     : PaidContentState::kNoPaidContentFound;
  }
  base::UmaHistogramEnumeration(kHistogramPaidContentPageLoad, state);

  ukm::builders::PaidContentPageLoad builder(delegate.GetPageUkmSourceId());
  builder.SetHasPaidContent(static_cast<int64_t>(state));
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace

void PaidContentPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordPaidContentPageLoad(GetDelegate(), has_paid_content_);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PaidContentPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordPaidContentPageLoad(GetDelegate(), has_paid_content_);
  return CONTINUE_OBSERVING;
}
