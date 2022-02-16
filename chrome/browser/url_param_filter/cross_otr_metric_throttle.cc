// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/cross_otr_metric_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_util.h"

namespace url_param_filter {

// static
std::unique_ptr<content::NavigationThrottle>
CrossOtrMetricNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The throttle should currently only apply to context-menu initiated
  // navigations, i.e., via Open Link in Incognito Window.
  if (!handle->WasStartedFromContextMenu()) {
    return nullptr;
  }
  // Verify that we have an initiator and it was not off the record (i.e., the
  // navigation switched).
  if (!handle->GetInitiatorFrameToken().has_value()) {
    return nullptr;
  }
  content::RenderProcessHost* initiator_process_host =
      content::RenderProcessHost::FromID(handle->GetInitiatorProcessID());
  if (!initiator_process_host) {
    return nullptr;
  }
  if (initiator_process_host->GetBrowserContext()->IsOffTheRecord()) {
    return nullptr;
  }
  // Finally, verify that the current browsing context is off the record.
  if (!handle->GetWebContents()->GetBrowserContext()->IsOffTheRecord()) {
    return nullptr;
  }
  return base::WrapUnique(new CrossOtrMetricNavigationThrottle(handle));
}

CrossOtrMetricNavigationThrottle::~CrossOtrMetricNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
CrossOtrMetricNavigationThrottle::WillProcessResponse() {
  const net::HttpResponseHeaders* headers =
      navigation_handle()->GetResponseHeaders();
  if (headers) {
    base::UmaHistogramSparse(
        "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental",
        net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
  }
  return content::NavigationThrottle::PROCEED;
}

const char* CrossOtrMetricNavigationThrottle::GetNameForLogging() {
  return "CrossOtrMetricNavigationThrottle";
}

CrossOtrMetricNavigationThrottle::CrossOtrMetricNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

}  // namespace url_param_filter
