// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

void PrerenderPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  did_fcp_ = true;
}

void PrerenderPageLoadMetricsObserver::OnStorageAccessed(
    const GURL& url,
    const GURL& first_party_url,
    bool blocked_by_policy,
    page_load_metrics::StorageType access_type) {
  if (access_type != page_load_metrics::StorageType::kLocalStorage ||
      did_local_storage_)
    return;

  // The purpose of this observer is to estimate how many prerendering pages
  // will use certain features. The plan for prerendering is to delay loading
  // of cross-origin iframes, so we want to ignore feature uses inside
  // cross-origin iframes. To do this, just check if the |url| is cross-origin
  // to |first_party_url|. This may not be an accurate count if a third-party
  // subframe embeds a first-party subframe, or if there is a way for a frame
  // to access cross-origin storage, but it's probably not significant.
  if (!url::IsSameOriginWith(url, first_party_url))
    return;

  did_local_storage_ = true;
  RecordFeatureUse(
      did_fcp_ ? blink::mojom::WebFeature::kLocalStorageFirstUsedAfterFcp
               : blink::mojom::WebFeature::kLocalStorageFirstUsedBeforeFcp);
}

void PrerenderPageLoadMetricsObserver::RecordFeatureUse(
    blink::mojom::WebFeature feature) {
  page_load_metrics::mojom::PageLoadFeatures page_load_features;
  page_load_features.features.push_back(feature);

  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      GetDelegate().GetWebContents()->GetMainFrame(), page_load_features);
}
