// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_METRICS_SERVICES_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_METRICS_CONTENT_METRICS_SERVICES_WEB_CONTENTS_OBSERVER_H_

#include "base/functional/callback.h"
#include "components/metrics/metrics_service_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace metrics {

class MetricsServicesWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetricsServicesWebContentsObserver> {
 public:
  using OnDidStartLoadingCb = base::RepeatingClosure;
  using OnDidStopLoadingCb = base::RepeatingClosure;
  using OnRendererUnresponsiveCb = base::RepeatingClosure;

  MetricsServicesWebContentsObserver(
      const MetricsServicesWebContentsObserver&) = delete;
  MetricsServicesWebContentsObserver& operator=(
      const MetricsServicesWebContentsObserver&) = delete;

  ~MetricsServicesWebContentsObserver() override;

 private:
  explicit MetricsServicesWebContentsObserver(
      content::WebContents* web_contents,
      OnDidStartLoadingCb did_start_loading_cb,
      OnDidStopLoadingCb did_stop_loading_cb,
      OnRendererUnresponsiveCb renderer_unresponsive_cb);
  friend class content::WebContentsUserData<MetricsServicesWebContentsObserver>;

  // content::WebContentsObserver:
  void DidStartLoading() override;
  void DidStopLoading() override;
  void OnRendererUnresponsive(content::RenderProcessHost* host) override;

  OnDidStartLoadingCb did_start_loading_cb_;
  OnDidStopLoadingCb did_stop_loading_cb_;
  OnRendererUnresponsiveCb renderer_unresponsive_cb_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_METRICS_SERVICES_WEB_CONTENTS_OBSERVER_H_
