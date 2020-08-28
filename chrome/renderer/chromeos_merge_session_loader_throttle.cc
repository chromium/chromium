// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chromeos_merge_session_loader_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

// static
base::TimeDelta MergeSessionLoaderThrottle::GetMergeSessionTimeout() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShortMergeSessionTimeoutForTest)) {
    return base::TimeDelta::FromSeconds(1);
  } else {
    return base::TimeDelta::FromSeconds(20);
  }
}

MergeSessionLoaderThrottle::MergeSessionLoaderThrottle(
    scoped_refptr<ChromeRenderThreadObserver::ChromeOSListener>
        chromeos_listener)
    : chromeos_listener_(std::move(chromeos_listener)) {}

MergeSessionLoaderThrottle::~MergeSessionLoaderThrottle() = default;

bool MergeSessionLoaderThrottle::MaybeDeferForMergeSession(
    const GURL& url,
    DelayedCallbackGroup::Callback resume_callback) {
  if (!chromeos_listener_ || !chromeos_listener_->IsMergeSessionRunning())
    return false;

  chromeos_listener_->RunWhenMergeSessionFinished(std::move(resume_callback));
  return true;
}

void MergeSessionLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  is_xhr_ = request->resource_type ==
            static_cast<int>(blink::mojom::ResourceType::kXhr);
  if (is_xhr_ && request->url.SchemeIsHTTPOrHTTPS() &&
      MaybeDeferForMergeSession(
          request->url,
          base::BindOnce(&MergeSessionLoaderThrottle::ResumeLoader,
                         weak_ptr_factory_.GetWeakPtr()))) {
    *defer = true;
  }
}

void MergeSessionLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& /* response_head */,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  if (is_xhr_ && redirect_info->new_url.SchemeIsHTTPOrHTTPS() &&
      MaybeDeferForMergeSession(
          redirect_info->new_url,
          base::BindOnce(&MergeSessionLoaderThrottle::ResumeLoader,
                         weak_ptr_factory_.GetWeakPtr()))) {
    *defer = true;
  }
}

void MergeSessionLoaderThrottle::DetachFromCurrentSequence() {}

void MergeSessionLoaderThrottle::ResumeLoader(
    DelayedCallbackGroup::RunReason run_reason) {
  LOG_IF(ERROR, run_reason == DelayedCallbackGroup::RunReason::TIMEOUT)
      << "Merge session loader throttle timeout.";
  DVLOG(1) << "Resuming deferred XHR request.";
  delegate_->Resume();
}
