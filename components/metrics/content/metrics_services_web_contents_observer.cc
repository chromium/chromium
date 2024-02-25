// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/metrics_services_web_contents_observer.h"

#include "components/metrics/metrics_service.h"

namespace metrics {

MetricsServicesWebContentsObserver::MetricsServicesWebContentsObserver(
    content::WebContents* web_contents,
    OnDidStartLoadingCb did_start_loading_cb,
    OnDidStopLoadingCb did_stop_loading_cb,
    OnRendererUnresponsiveCb renderer_unresponsive_cb)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<MetricsServicesWebContentsObserver>(
          *web_contents),
      did_start_loading_cb_(std::move(did_start_loading_cb)),
      did_stop_loading_cb_(std::move(did_stop_loading_cb)),
      renderer_unresponsive_cb_(std::move(renderer_unresponsive_cb)) {}
MetricsServicesWebContentsObserver::~MetricsServicesWebContentsObserver() =
    default;

void MetricsServicesWebContentsObserver::DidStartLoading() {
  did_start_loading_cb_.Run();
}

void MetricsServicesWebContentsObserver::DidStopLoading() {
  did_stop_loading_cb_.Run();
}

void MetricsServicesWebContentsObserver::OnRendererUnresponsive(
    content::RenderProcessHost* host) {
  renderer_unresponsive_cb_.Run();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MetricsServicesWebContentsObserver);

}  // namespace metrics
