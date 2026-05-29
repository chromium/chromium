// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/ash_merge_session_loader_throttle.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/google/core/common/google_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace {
bool ShouldDelayUrl(const GURL& url) {
  // TODO(b/320891641) - We should ideally call
  // `ash::merge_session_throttling_utils::ShouldDelayUrl()` but can't because
  // of include dependencies issues. Figure out how to extract the common
  // components.
  // We need to throttle requests to Google web properties while cookie minting
  // is in progress (Signalled by
  // `chromeos_listener_->IsMergeSessionRunning()`). If we do not do this, users
  // will get a "Sign in to Google" prompt while visiting Google web properties
  // - which is not the expected user experience on ChromeOS / Ash. Users expect
  // a Single Sign On experience on ChromeOS - i.e. when they sign-in to
  // ChromeOS at the ChromeOS login screen, they expect to be signed into Google
  // web properties inside their session. Since there can be a delay in minting
  // Google cookies on the user's behalf - and they can navigate to Google web
  // properties in the browser while cookies are being minted, we need to
  // throttle these requests. At the same time, we do not want to throttle
  // requests for non-Google web properties (see http://b/315072145 [note:
  // Google-internal link, but the context matches what's described in this
  // comment]).
  return google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                        google_util::ALLOW_NON_STANDARD_PORTS);
}
}  // namespace

// static
base::TimeDelta AshMergeSessionLoaderThrottle::GetMergeSessionTimeout() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShortMergeSessionTimeoutForTest)) {
    return base::Seconds(1);
  } else {
    return base::Seconds(20);
  }
}

AshMergeSessionLoaderThrottle::AshMergeSessionLoaderThrottle(
    scoped_refptr<ChromeRenderThreadObserver::ChromeOSListener>
        chromeos_listener)
    : chromeos_listener_(std::move(chromeos_listener)) {}

AshMergeSessionLoaderThrottle::~AshMergeSessionLoaderThrottle() = default;

bool AshMergeSessionLoaderThrottle::MaybeDeferForMergeSession(
    const GURL& url,
    DelayedCallbackGroup::Callback resume_callback) {
  if (!chromeos_listener_ || !chromeos_listener_->IsMergeSessionRunning()) {
    return false;
  }

  if (!ShouldDelayUrl(url)) {
    return false;
  }

  chromeos_listener_->RunWhenMergeSessionFinished(std::move(resume_callback));
  return true;
}

void AshMergeSessionLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  is_xhr_ = request->resource_type ==
            static_cast<int>(blink::mojom::ResourceType::kXhr);
  if (is_xhr_ && request->url.SchemeIsHTTPOrHTTPS() &&
      MaybeDeferForMergeSession(
          request->url,
          base::BindOnce(&AshMergeSessionLoaderThrottle::ResumeLoader,
                         weak_ptr_factory_.GetWeakPtr()))) {
    *defer = true;
  }
}

void AshMergeSessionLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& /* response_head */,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  if (is_xhr_ && redirect_info->new_url.SchemeIsHTTPOrHTTPS() &&
      MaybeDeferForMergeSession(
          redirect_info->new_url,
          base::BindOnce(&AshMergeSessionLoaderThrottle::ResumeLoader,
                         weak_ptr_factory_.GetWeakPtr()))) {
    *defer = true;
  }
}

void AshMergeSessionLoaderThrottle::DetachFromCurrentSequence() {}

void AshMergeSessionLoaderThrottle::ResumeLoader(
    DelayedCallbackGroup::RunReason run_reason) {
  LOG_IF(ERROR, run_reason == DelayedCallbackGroup::RunReason::TIMEOUT)
      << "Merge session loader throttle timeout.";
  DVLOG(1) << "Resuming deferred XHR request.";
  delegate_->Resume();
}
